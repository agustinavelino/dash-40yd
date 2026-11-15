// =====================================================================
// DASH Timing System - Experimento 1 - Control de interferencia mutua
//
// QUE PROBLEMA RESUELVE (Documento Maestro v2.3, 16.3, "control previo
// obligatorio")
//   Los dos VL53L1X emiten a 940 nm hacia el mismo punto. Si uno ve el
//   pulso del otro, la distancia se degrada y el Exp. 1 mediria esa
//   interferencia en lugar de la cuantizacion. Se mide la distancia a un
//   blanco FIJO con cada sensor solo y con los dos encendidos.
//
// COMO FUNCIONA
//   El MISMO binario va en las dos placas; el rol sale de la MAC. El hub
//   manda al nodo por ESP-NOW: arranca (1), para (2), reporta y reinicia
//   estadisticas (3). Por repeticion:
//     F1 hub solo   (nodo parado)       -> estadisticas del hub
//     F2 ambos      (los dos midiendo)  -> del hub y del nodo
//     F3 nodo solo  (hub parado)        -> del nodo
//   N_FASE muestras por fase (~3.5 s), REPETICIONES veces. La distancia se
//   lee por sondeo (read() bloqueante): aqui no importa el tiempo, solo la
//   distancia, asi que no hay ISR.
//
// CRITERIO (PROPUESTO, es una preferencia, no viene de una medicion):
//   "OK" si con el otro encendido: validas >= 99 %, |delta media| <= 10 mm
//   y sigma <= 2 x sigma solo + 1 mm. Si no, separar los sensores unos
//   grados y repetir.
//
// COMO USARLO
//   Cartulina fija a ~50 cm frente a los dos sensores, cubriendo los dos
//   haces. Flashear este sketch en las dos placas; logger en el hub con
//   --fin "^# fin$" --max-s 120.
//
// Cableado: igual que exp1_hub / exp1_nodo (SDA 21, SCL 22, XSHUT 17).
// =====================================================================

#include <WiFi.h>
#include <esp_now.h>
#include <esp_mac.h>
#include <Wire.h>
#include <VL53L1X.h>

uint8_t macHub[]  = {0xA4, 0xF0, 0x0F, 0x64, 0xAC, 0x68};
uint8_t macNodo[] = {0xB0, 0xCB, 0xD8, 0xCA, 0x11, 0xF0};

#define PIN_SDA     21
#define PIN_SCL     22
#define PIN_XSHUT   17
#define IMP_MS      25
#define BUDGET_US   20000
#define N_FASE      150
#define N_DESCARTE  15
#define REPETICIONES 3

// tipo 20 = comando del hub (cmd 1 arranca, 2 para, 3 reporta y reinicia)
// tipo 21 = reporte del nodo (sus estadisticas desde el ultimo reinicio)
typedef struct __attribute__((packed)) {
  uint8_t  tipo, cmd;
  uint16_t n, nVal;
  float    media, sigma;
  uint16_t dmin, dmax;
} Ctl;

struct Stats {
  uint16_t n, nVal, mn, mx;
  double   s, s2;
  void reset() { n = nVal = 0; s = s2 = 0; mn = 65535; mx = 0; }
  void add(uint16_t d, uint8_t st) {
    n++;
    if (st != 0) return;                       // solo RangeValid
    nVal++; s += d; s2 += (double)d * d;
    if (d < mn) mn = d;
    if (d > mx) mx = d;
  }
  float media() const { return nVal ? s / nVal : NAN; }
  float sigma() const {
    if (nVal < 2) return NAN;
    double m = s / nVal;
    return sqrt((s2 - nVal * m * m) / (nVal - 1));
  }
};

VL53L1X sensor;
bool soyHub = false;
uint8_t *macOtro;

// --- nodo ---
volatile uint8_t cmdPendiente = 0;
bool  corriendo = false;
Stats stNodo;

// --- hub ---
volatile bool hayReporte = false;
Ctl   reporte;

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(Ctl)) return;
  Ctl c;
  memcpy(&c, data, sizeof(c));
  if (!soyHub && c.tipo == 20) cmdPendiente = c.cmd;             // el loop lo ejecuta (I2C fuera del callback)
  if (soyHub && c.tipo == 21) { reporte = c; hayReporte = true; }
}

void resetSensor() {                           // regla 9
  pinMode(PIN_XSHUT, OUTPUT);
  digitalWrite(PIN_XSHUT, LOW);
  delay(10);
  pinMode(PIN_XSHUT, INPUT);
  delay(5);
}

Ctl aCtl(const Stats &s) {
  Ctl c = {};
  c.tipo = 21; c.n = s.n; c.nVal = s.nVal;
  c.media = s.media(); c.sigma = s.sigma();
  c.dmin = s.mn; c.dmax = s.mx;
  return c;
}

// Hub: manda un comando y espera el reporte (hasta 5 intentos)
bool comando(uint8_t cmd, Ctl &out) {
  for (int i = 0; i < 5; i++) {
    hayReporte = false;
    Ctl c = {}; c.tipo = 20; c.cmd = cmd;
    esp_now_send(macOtro, (uint8_t *)&c, sizeof(c));
    uint32_t t0 = millis();
    while (!hayReporte && millis() - t0 < 300) delay(2);
    if (hayReporte) { out = reporte; return true; }
  }
  return false;
}

Stats medir(int n) {
  Stats s; s.reset();
  for (int i = 0; i < n; i++) {
    uint16_t d = sensor.read();
    s.add(d, (uint8_t)sensor.ranging_data.range_status);
  }
  return s;
}

// Resultados (se imprimen al final)
struct Fila { int rep; const char *fase, *quien; Ctl c; };
Fila filas[REPETICIONES * 4];
int  nFilas = 0;
void guardar(int rep, const char *fase, const char *quien, const Ctl &c) {
  filas[nFilas++] = {rep, fase, quien, c};
}

void setup() {
  Serial.begin(921600);
  delay(1000);

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  soyHub = memcmp(mac, macHub, 6) == 0;
  bool soyNodo = memcmp(mac, macNodo, 6) == 0;
  macOtro = soyHub ? macNodo : macHub;
  if (!soyHub && !soyNodo) { Serial.println("# MAC desconocida: ni hub ni nodo"); return; }

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  resetSensor();
  sensor.setTimeout(500);
  if (!sensor.init()) {
    Serial.printf("# FALLO init() del sensor del %s\n", soyHub ? "hub" : "nodo");
    Serial.println("# fin");
    return;
  }
  sensor.setDistanceMode(VL53L1X::Short);
  sensor.setMeasurementTimingBudget(BUDGET_US);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) { Serial.println("error init"); return; }
  esp_now_register_recv_cb(onRecv);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, macOtro, 6);
  peer.channel = 0; peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) { Serial.println("error peer"); return; }

  Serial.printf("# control_interferencia | soy %s\n", soyHub ? "hub" : "nodo");
  stNodo.reset();
  if (!soyHub) return;                         // el nodo trabaja en loop()

  // ---------------- hub ----------------
  Ctl r;
  Serial.println("# esperando al nodo...");
  while (!comando(2, r)) delay(500);

  for (int rep = 1; rep <= REPETICIONES; rep++) {
    // F1: hub solo
    comando(2, r);
    sensor.startContinuous(IMP_MS);
    medir(N_DESCARTE);
    guardar(rep, "solo", "hub", aCtl(medir(N_FASE)));

    // F2: ambos
    comando(1, r);
    medir(N_DESCARTE);                         // el nodo se estabiliza mientras
    comando(3, r);                             // reinicia sus estadisticas
    Stats h = medir(N_FASE);
    bool ok = comando(3, r);
    guardar(rep, "ambos", "hub", aCtl(h));
    if (ok) guardar(rep, "ambos", "nodo", r);

    // F3: nodo solo
    sensor.stopContinuous();
    delay(100);
    comando(3, r);
    delay(N_FASE * 24);
    ok = comando(2, r);
    if (ok) guardar(rep, "solo", "nodo", r);
  }

  Serial.println("# blanco fijo | modo corto, budget 20 ms, IMP 25 | solo status 0 en media y sigma");
  Serial.println("C,rep,fase,sensor,n,validas,media_mm,sigma_mm,min_mm,max_mm");
  for (int i = 0; i < nFilas; i++) {
    Fila &f = filas[i];
    Serial.printf("C,%d,%s,%s,%u,%u,%.1f,%.2f,%u,%u\n", f.rep, f.fase, f.quien,
                  f.c.n, f.c.nVal, f.c.media, f.c.sigma, f.c.dmin, f.c.dmax);
  }

  // Veredicto con el criterio PROPUESTO del encabezado, por sensor y repeticion
  for (int rep = 1; rep <= REPETICIONES; rep++) {
    for (int q = 0; q < 2; q++) {
      const char *quien = q ? "nodo" : "hub";
      Ctl *solo = nullptr, *ambos = nullptr;
      for (int i = 0; i < nFilas; i++) {
        if (filas[i].rep != rep || strcmp(filas[i].quien, quien)) continue;
        if (!strcmp(filas[i].fase, "solo")) solo = &filas[i].c; else ambos = &filas[i].c;
      }
      if (!solo || !ambos) { Serial.printf("# rep %d %s: faltan datos (reporte del nodo perdido)\n", rep, quien); continue; }
      float dm = ambos->media - solo->media;
      float pv = 100.0f * ambos->nVal / (ambos->n ? ambos->n : 1);
      bool ok = pv >= 99.0f && fabsf(dm) <= 10.0f && ambos->sigma <= 2.0f * solo->sigma + 1.0f;
      Serial.printf("# rep %d %-4s: validas %.0f %% | delta media %+.1f mm | sigma %.2f -> %.2f mm | %s\n",
                    rep, quien, pv, dm, solo->sigma, ambos->sigma, ok ? "OK" : "REVISAR");
    }
  }
  Serial.println("# fin");
}

void loop() {
  if (soyHub) return;

  uint8_t c = cmdPendiente;
  if (c) {
    cmdPendiente = 0;
    if (c == 1 && !corriendo) { sensor.startContinuous(IMP_MS); corriendo = true; }
    if (c == 2 && corriendo)  { sensor.stopContinuous(); corriendo = false; }
    Ctl rep = aCtl(stNodo);
    esp_now_send(macHub, (uint8_t *)&rep, sizeof(rep));
    stNodo.reset();
  }
  if (corriendo) {
    uint16_t d = sensor.read();
    stNodo.add(d, (uint8_t)sensor.ranging_data.range_status);
  }
}
