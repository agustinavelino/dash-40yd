// =====================================================================
// DASH Timing System - Experimento 1 - NODO (placa B del Exp. 0)
//
// Base: experimentos/exp0/prueba_0_B. Responde pings igual que en el
// Exp. 0 (regla 4) y espera el handshake (tipo 4) para enganchar su ISR.
// En lugar del pulso por cable, el evento lo genera su VL53L1X con el mismo
// detector que el hub (ver exp1_hub.ino). Por cada deteccion manda al hub
// un mensaje tipo 5 con el timestamp de la ISR (reloj del nodo) y la traza.
// Guarda ademas una copia local que imprime al recibir el tipo 6 (fin):
// es el respaldo si un mensaje no llego al hub. Lo agregado lleva [exp1].
//
// CABLEADO
//   VL53L1X: VIN->3V3  GND->GND  SDA->GPIO21  SCL->GPIO22
//            GPIO1(INT)->GPIO16  XSHUT->GPIO17
//   Sin LED y sin cable hacia el hub.
//
// Libreria: "VL53L1X" de Pololu 1.3.1
// =====================================================================

#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <VL53L1X.h>

uint8_t macHub[] = {0xA4, 0xF0, 0x0F, 0x64, 0xAC, 0x68};   // MAC STA del hub

// --- sensor y detector [exp1]: mismos valores que el hub ---
#define PIN_SDA     21
#define PIN_SCL     22
#define PIN_INT     16
#define PIN_XSHUT   17
#define IMP_MS      25
#define BUDGET_US   20000
#define UMBRAL_MM   1000
#define N_LIBRE     5
#define N_DESCARTE  10
#define PRE         10
#define POST        5
#define TRAZA       (PRE + 1 + POST)
#define PERIODO_US  23300
#define LIMITE_RESCATE_US (2LL * IMP_MS * 1000)
#define M_RESCATE 1
#define M_DOBLE   2
#define M_TARDIA  4
#define MAX_EVENTOS 250

// tipo: 1=ping  2=pong  4=handshake de arranque  6=fin de corrida [exp1]
typedef struct __attribute__((packed)) {
  uint8_t  tipo;
  uint32_t seq;
  int64_t  t1, t2, t3;
} Mensaje;

// [exp1] tipo 5: deteccion con traza (mismo struct que en el hub)
typedef struct __attribute__((packed)) {
  uint8_t  tipo;
  uint32_t seq;
  int64_t  tDet;
  uint32_t nMuestras;
  uint16_t nRescates, nDobles;
  int32_t  dt[TRAZA];
  uint16_t dist[TRAZA];
  uint8_t  st[TRAZA];
  uint8_t  marca[TRAZA];
} Traza;

volatile bool pedidoArranque = false;          // bandera del handshake
volatile bool pedidoFin = false;               // [exp1]
bool          interrupcionLista = false;
bool          terminado = false;

VL53L1X sensor;
volatile int64_t  tINT = 0;
volatile bool     nueva = false;
volatile uint32_t nIsr = 0;

// ISR: SOLO captura el timestamp y sale (igual que en el hub).
void IRAM_ATTR isrINT() {
  tINT = esp_timer_get_time();
  nueva = true;
  nIsr++;
}

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  int64_t t2 = esp_timer_get_time();           // PRIMERA instruccion
  if (len != sizeof(Mensaje)) return;
  Mensaje m;
  memcpy(&m, data, sizeof(m));                 // hay que llenar m ANTES de usarlo

  if (m.tipo == 4) {                           // handshake: el hub dice "arranca"
    pedidoArranque = true;
    return;
  }
  if (m.tipo == 6) {                           // [exp1] fin de la corrida
    pedidoFin = true;
    return;
  }

  if (m.tipo != 1) return;                     // solo responde pings
  m.tipo = 2;
  m.t2   = t2;
  m.t3   = esp_timer_get_time();
  esp_now_send(macHub, (uint8_t *)&m, sizeof(m));
}

// --- detector [exp1] (igual que en el hub) ---
struct Muestra { int64_t t; uint16_t d; uint8_t s; uint8_t m; };
Muestra  anillo[PRE + 1];
uint32_t nMuestras = 0, nRescates = 0, nDobles = 0, nTardias = 0, nIsrPrevio = 0;
int      libres = 0, postFaltan = 0, descartes = 0;
int64_t  tUltima = 0;
Traza    actual;                               // evento en construccion
int      eIdx = 0;

Traza    guardados[MAX_EVENTOS];               // respaldo local
int8_t   envioOk[MAX_EVENTOS];
int      nE = 0, nEFuera = 0, nEnvioFallido = 0;
uint32_t seqDet = 0;

void resetSensor() {                           // regla 9
  pinMode(PIN_XSHUT, OUTPUT);
  digitalWrite(PIN_XSHUT, LOW);
  delay(10);
  pinMode(PIN_XSHUT, INPUT);
  delay(5);
}

void limpiarIntPendiente() {
  for (int i = 0; i < 6 && digitalRead(PIN_INT) == LOW; i++) {
    sensor.read(false);
    delay(5);
  }
}

void cerrarEvento() {
  actual.tipo = 5;
  actual.nMuestras = nMuestras;
  actual.nRescates = (uint16_t)nRescates;
  actual.nDobles   = (uint16_t)nDobles;
  esp_err_t r = esp_now_send(macHub, (uint8_t *)&actual, sizeof(actual));
  if (r != ESP_OK) nEnvioFallido++;
  if (nE < MAX_EVENTOS) {
    memcpy(&guardados[nE], &actual, sizeof(actual));
    envioOk[nE] = (r == ESP_OK);
    nE++;
  } else {
    nEFuera++;
  }
}

void procesarMuestra(int64_t t, uint16_t d, uint8_t s, uint8_t m) {
  nMuestras++;
  Muestra &w = anillo[(nMuestras - 1) % (PRE + 1)];
  w.t = t; w.d = d; w.s = s; w.m = m;

  if (postFaltan > 0) {
    actual.dt[eIdx] = (int32_t)(t - actual.tDet); actual.dist[eIdx] = d;
    actual.st[eIdx] = s; actual.marca[eIdx] = m;
    eIdx++;
    if (--postFaltan == 0) cerrarEvento();
  }

  bool blanco = (s == 0 && d < UMBRAL_MM);
  if (blanco && libres >= N_LIBRE && postFaltan == 0 && nMuestras > PRE) {
    actual.tDet = t;
    actual.seq  = ++seqDet;
    for (int k = 0; k <= PRE; k++) {
      Muestra &a = anillo[(nMuestras - 1 - PRE + k) % (PRE + 1)];
      actual.dt[k] = (int32_t)(a.t - t); actual.dist[k] = a.d;
      actual.st[k] = a.s; actual.marca[k] = a.m;
    }
    eIdx = PRE + 1;
    postFaltan = POST;
  }
  libres = blanco ? 0 : libres + 1;
}

void atenderSensor() {
  uint8_t m = 0;
  int64_t t;
  if (nueva) {
    noInterrupts();
    nueva = false;
    t = tINT;
    uint32_t n = nIsr;
    interrupts();
    if (n - nIsrPrevio > 1) m |= M_DOBLE;
    nIsrPrevio = n;
  } else if (esp_timer_get_time() - tUltima > LIMITE_RESCATE_US && digitalRead(PIN_INT) == LOW) {
    t = esp_timer_get_time();                  // red de seguridad (regla 7)
    m |= M_RESCATE;
  } else {
    return;
  }
  tUltima = t;
  if (esp_timer_get_time() - t > (PERIODO_US * 3) / 2) m |= M_TARDIA;

  uint16_t d = sensor.read(false);
  uint8_t  s = (uint8_t)sensor.ranging_data.range_status;
  if (descartes < N_DESCARTE) { descartes++; return; }
  if (m & M_RESCATE) nRescates++;
  if (m & M_DOBLE)   nDobles++;
  if (m & M_TARDIA)  nTardias++;
  procesarMuestra(t, d, s, m);
}

void lineaConChecksum(const char *s) {
  uint8_t x = 0;
  for (const char *p = s; *p; p++) x ^= (uint8_t)*p;
  Serial.printf("%s*%02X\n", s, x);
}

// Respaldo: mismas columnas que las lineas N del hub, sin t_rx (se pone -1)
void imprimirRespaldo() {
  char l[640];
  Serial.println("# respaldo del nodo | NL,k,seq_nodo,t_det_nodo_us,envio_ok,muestras_nodo,rescates,dobles,dt(16),dist(16),status(16),marca(16)");
  for (int k = 0; k < nE; k++) {
    Traza &r = guardados[k];
    int n = snprintf(l, sizeof(l), "NL,%d,%lu,%lld,%d,%lu,%u,%u", k, (unsigned long)r.seq,
                     (long long)r.tDet, envioOk[k], (unsigned long)r.nMuestras, r.nRescates, r.nDobles);
    for (int j = 0; j < TRAZA; j++) n += snprintf(l + n, sizeof(l) - n, "%c%ld", j ? ';' : ',', (long)r.dt[j]);
    for (int j = 0; j < TRAZA; j++) n += snprintf(l + n, sizeof(l) - n, "%c%u",  j ? ';' : ',', r.dist[j]);
    for (int j = 0; j < TRAZA; j++) n += snprintf(l + n, sizeof(l) - n, "%c%u",  j ? ';' : ',', r.st[j]);
    for (int j = 0; j < TRAZA; j++) n += snprintf(l + n, sizeof(l) - n, "%c%u",  j ? ';' : ',', r.marca[j]);
    lineaConChecksum(l);
  }
  snprintf(l, sizeof(l), "SN,%lu,%lu,%lu,%lu,%d,%d,%d", (unsigned long)nMuestras, (unsigned long)nRescates,
           (unsigned long)nDobles, (unsigned long)nTardias, nE, nEFuera, nEnvioFallido);
  Serial.println("# SN,muestras,rescates,dobles,tardias,detecciones,fuera,envios_fallidos");
  lineaConChecksum(l);
  Serial.println("# fin");
}

void setup() {
  Serial.begin(921600);
  delay(1000);

  // [exp1] sensor: se arranca aqui; la ISR espera al handshake
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  pinMode(PIN_INT, INPUT_PULLUP);
  resetSensor();
  sensor.setTimeout(500);
  if (!sensor.init()) {
    Serial.println("# FALLO init() del sensor del nodo. Revisa VIN/GND, SDA 21, SCL 22, XSHUT 17.");
    terminado = true;
    return;
  }
  sensor.setDistanceMode(VL53L1X::Short);
  sensor.setMeasurementTimingBudget(BUDGET_US);
  sensor.startContinuous(IMP_MS);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) { Serial.println("error init"); terminado = true; return; }
  esp_now_register_recv_cb(onRecv);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, macHub, 6);
  peer.channel = 0; peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) { Serial.println("error peer"); terminado = true; return; }

  Serial.printf("# exp1 nodo | STA %s | sensor OK | esperando handshake\n", WiFi.macAddress().c_str());
}

void loop() {
  if (terminado) return;

  // Enganchar la ISR aqui (no dentro del callback de ESP-NOW). Regla 7:
  // despues de startContinuous() y con INT limpio.
  if (pedidoArranque && !interrupcionLista) {
    pedidoArranque = false;
    limpiarIntPendiente();
    nueva = false;
    nIsrPrevio = nIsr;
    attachInterrupt(digitalPinToInterrupt(PIN_INT), isrINT, FALLING);
    tUltima = esp_timer_get_time();
    interrupcionLista = true;
    Serial.println("# handshake recibido, detectando");
  }

  if (pedidoFin && interrupcionLista) {
    detachInterrupt(digitalPinToInterrupt(PIN_INT));
    sensor.stopContinuous();
    imprimirRespaldo();
    terminado = true;
    return;
  }

  if (interrupcionLista) atenderSensor();
}
