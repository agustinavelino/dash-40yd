// =====================================================================
// DASH Timing System - Experimento 1 - HUB (placa A del Exp. 0)
//
// QUE MIDE (Documento Maestro v2.3, 16.3)
//   Dos VL53L1X apilados ven el mismo cruce de una cartulina; uno va en
//   esta placa y el otro en el nodo. El valor verdadero de la diferencia
//   entre sus timestamps es cero por construccion: lo que salga es error,
//   y casi todo es del sensor:
//       sigma_sensor = sqrt((sigma_exp1^2 - sigma_exp0^2) / 2)
//   Prediccion escrita ANTES de medir (no se ajusta): sigma_exp1 = 9.5 ms
//   con ventanas uniformes, 10.7 ms con la tasa de anomalos de la 9.2.
//
// BASE
//   Resync, handshake (tipo 4) y formula: experimentos/exp0/prueba_0_A sin
//   cambios (regla 4). Se quita el pulso por cable: el evento lo genera el
//   sensor. Sensor como en corrida_cielo.ino: reset por XSHUT, init una sola
//   vez, modo corto, budget 20 ms, IMP 25 ms, ISR por FALLING enganchada con
//   INT limpio y red de seguridad. Todo lo agregado lleva la marca [exp1].
//
// DETECCION (identica en hub y nodo)
//   El sensor interrumpe en cada medicion (~23.3 ms). Una deteccion es la
//   primera muestra con blanco (status 0 y distancia < UMBRAL_MM) despues
//   de al menos N_LIBRE muestras sin blanco; su timestamp es el de la ISR
//   de esa muestra. Supuesto sin verificar: equivale a que el chip aplicara
//   el umbral, porque en los dos casos el INT sale al terminar la medicion.
//   Por cada deteccion se guarda una traza de PRE muestras antes, la
//   deteccion y POST despues (tiempo, distancia, status, marca). De ahi
//   salen el intervalo previo a la deteccion (lo pide la 16.3) y la
//   interpolacion del flanco (9.5) sin otra corrida.
//
// CRUCES GUIADOS (referencia registrada, ver feedback del 24-oct)
//   LED (GPIO18 externo y GPIO2): parpadeo rapido PREVIO_MS = preparate.
//   Despues, N_CICLOS ciclos de CUE_PERIODO_MS; encendido CUE_ENCENDIDO_MS
//   = un cruce. Cada deteccion guarda el ciclo y si el LED estaba encendido.
//   El resync periodico (cada >= 30 s) solo se lanza al final del ciclo,
//   con el LED apagado: durante la rafaga (~200 ms de delay) el loop no lee
//   el sensor y una deteccion en ese hueco tendria un timestamp que no es
//   de su lectura. Solo cambia CUANDO se llama a hacerResync, no el algoritmo.
//
// SALIDA (diferida, regla 5; cada linea con checksum *HH)
//   R = rafagas de resync, H = detecciones del hub, N = detecciones del
//   nodo recibidas por ESP-NOW, S = resumen, y al final "# fin".
//   El emparejamiento por proximidad (+-100 ms, regla 12) y la sigma los
//   hace analizar_exp1.py sobre el crudo, no el firmware.
//
// CABLEADO (igual que corrida_cielo; el nodo igual pero sin LED)
//   VL53L1X: VIN->3V3  GND->GND  SDA->GPIO21  SCL->GPIO22
//            GPIO1(INT)->GPIO16  XSHUT->GPIO17
//   LED: GPIO18 -> 330 ohm -> anodo, catodo -> GND
//   Sin cable entre placas: hablan solo por ESP-NOW.
//
// Logger: --fin "^# fin$" --max-s 1100 --idle-s 1000 (calla ~13 min).
// Libreria: "VL53L1X" de Pololu 1.3.1
// =====================================================================

#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <VL53L1X.h>

uint8_t macNodo[] = {0xB0, 0xCB, 0xD8, 0xCA, 0x11, 0xF0};  // MAC STA del nodo

// --- resync: identico al Exp. 0 ---
#define BURST       7      // pings por rafaga de resync
#define ESPACIADO_MS 20    // separacion entre pings de la rafaga
#define PERIODO_RESYNC_US 30000000LL   // 30 s

// --- sensor [exp1] ---
#define PIN_SDA     21
#define PIN_SCL     22
#define PIN_INT     16
#define PIN_XSHUT   17
#define IMP_MS      25      // ADR-011
#define BUDGET_US   20000
#define UMBRAL_MM   1000    // blanco = status 0 y mas cerca que esto (alcance 1.38 m)
#define N_LIBRE     5       // muestras sin blanco antes de aceptar una deteccion (~116 ms)
#define N_DESCARTE  10      // el arranque del continuo no cuenta
#define PRE         10      // muestras de la traza antes de la deteccion
#define POST        5       // ... y despues
#define TRAZA       (PRE + 1 + POST)
#define PERIODO_US  23300   // periodo medido a IMP 25; solo para marcar lecturas tardias
#define LIMITE_RESCATE_US (2LL * IMP_MS * 1000)   // regla 7: > 2 periodos sin INT

// marcas por muestra (bits)
#define M_RESCATE 1         // timestamp de la red de seguridad, no de la ISR
#define M_DOBLE   2         // la ISR vio mas de un flanco desde la lectura anterior
#define M_TARDIA  4         // se leyo mas de 1.5 periodos despues de la ISR

// --- cruces guiados [exp1] ---
#define PIN_LED     2
#define PIN_CUE     18
#define PREVIO_MS   4000    // parpadeo rapido: preparate
#define CUE_PERIODO_MS   3500
#define CUE_ENCENDIDO_MS 1000
#define RESYNC_DESDE_MS  2300   // ventana del ciclo donde se permite el resync
#define RESYNC_HASTA_MS  3100
#define N_CICLOS    220     // ~12.8 min; se esperan >= 200 pares
#define COLA_MS     1500    // tras el ultimo ciclo: cerrar trazas y recibir las del nodo

#define MAX_EVENTOS 250
#define MAX_RESYNC  40
#define COLA_RX     8

// tipo: 1=ping  2=pong  4=handshake de arranque  6=fin de corrida [exp1]
typedef struct __attribute__((packed)) {
  uint8_t  tipo;
  uint32_t seq;
  int64_t  t1;
  int64_t  t2;
  int64_t  t3;
} Mensaje;

// [exp1] tipo 5: deteccion del nodo con su traza (149 bytes < 250 de ESP-NOW)
typedef struct __attribute__((packed)) {
  uint8_t  tipo;
  uint32_t seq;             // numero de deteccion en el nodo (informativo)
  int64_t  tDet;            // ISR de la deteccion, reloj del nodo
  uint32_t nMuestras;       // muestras del nodo hasta ahora
  uint16_t nRescates, nDobles;
  int32_t  dt[TRAZA];       // t_k - tDet (us)
  uint16_t dist[TRAZA];
  uint8_t  st[TRAZA];
  uint8_t  marca[TRAZA];
} Traza;

// --- estado del resync (Exp. 0) ---
volatile int64_t  offsets[BURST], delays[BURST];
volatile bool     recibidoP[BURST];
volatile uint32_t seqBase = 1;
volatile int64_t  offsetActual = 0;
volatile bool     hayOffset = false;

// --- registro de rafagas [exp1] ---
struct RegResync { int64_t t, offset, rtt; uint8_t pongs; };
RegResync regR[MAX_RESYNC];
int nR = 0;

// --- sensor [exp1] ---
VL53L1X sensor;
volatile int64_t  tINT = 0;
volatile bool     nueva = false;
volatile uint32_t nIsr = 0;

// ISR: SOLO captura el timestamp y sale. Nada de I2C, Serial ni radio aqui.
void IRAM_ATTR isrINT() {
  tINT = esp_timer_get_time();
  nueva = true;
  nIsr++;
}

// --- detector [exp1] (igual en el nodo) ---
struct Muestra { int64_t t; uint16_t d; uint8_t s; uint8_t m; };
Muestra  anillo[PRE + 1];
uint32_t nMuestras = 0, nRescates = 0, nDobles = 0, nTardias = 0, nIsrPrevio = 0;
int      libres = 0, postFaltan = 0, descartes = 0;
int64_t  tUltima = 0;
// evento en construccion
int64_t  eT, eOffset; int32_t eDt[TRAZA]; uint16_t eD[TRAZA]; uint8_t eS[TRAZA], eM[TRAZA];
int      eIdx; int16_t eCiclo; uint8_t eCue;

// --- detecciones del hub ---
struct EvHub { int64_t tDet, offset; int16_t ciclo; uint8_t cue;
               int32_t dt[TRAZA]; uint16_t d[TRAZA]; uint8_t s[TRAZA], m[TRAZA]; };
// Los dos arreglos (~78 KB) no caben en la DRAM estatica junto con WiFi
// (el enlazador se pasaba 3.9 KB): se reservan en el heap en setup().
EvHub *evH = nullptr;
int nH = 0, nHFuera = 0;

// --- detecciones del nodo (el callback las encola, el loop las guarda) ---
struct EvNodo { Traza tr; int64_t tRx; };
EvNodo *evN = nullptr;
int nN = 0, nNFuera = 0;
Traza   colaTr[COLA_RX];
int64_t colaRx[COLA_RX];
volatile uint8_t  colaIn = 0, colaOut = 0;
volatile uint32_t nColaLlena = 0;

// --- cruces guiados ---
uint32_t tCue0 = 0, tFin = 0;
int16_t  cicloAct = -1;
uint8_t  cueAct = 0;
enum { ARRANQUE, CORRIENDO, COLA, TERMINADO } fase = ARRANQUE;

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  int64_t t4 = esp_timer_get_time();           // PRIMERA instruccion

  // [exp1] deteccion del nodo: copiar a la cola y salir
  if (len == sizeof(Traza) && data[0] == 5) {
    uint8_t sig = (colaIn + 1) % COLA_RX;
    if (sig == colaOut) { nColaLlena++; return; }
    memcpy(&colaTr[colaIn], data, sizeof(Traza));
    colaRx[colaIn] = t4;
    colaIn = sig;
    return;
  }

  if (len != sizeof(Mensaje)) return;
  Mensaje m;
  memcpy(&m, data, sizeof(m));

  if (m.tipo == 2) {                           // pong: respuesta del resync
    uint32_t idx = m.seq - seqBase;
    if (idx >= BURST) return;                  // pong viejo, fuera de la rafaga
    offsets[idx]   = ((m.t2 - m.t1) + (m.t3 - t4)) / 2;
    delays[idx]    = (t4 - m.t1) - (m.t3 - m.t2);   // round-trip puro
    recibidoP[idx] = true;
  }
}

uint32_t seqGlobal = 0;
int64_t  ultimoResync = 0;

// Rafaga de BURST pings; se queda con el de menor round-trip. (Exp. 0)
void hacerResync() {
  int64_t tInicio = esp_timer_get_time();      // [exp1] registro
  for (int i = 0; i < BURST; i++) recibidoP[i] = false;
  seqBase = seqGlobal + 1;

  for (int i = 0; i < BURST; i++) {
    seqGlobal++;
    Mensaje m = {};
    m.tipo = 1;
    m.seq  = seqGlobal;
    m.t1   = esp_timer_get_time();
    esp_now_send(macNodo, (uint8_t *)&m, sizeof(m));
    delay(ESPACIADO_MS);
  }
  delay(60);                                   // margen para rezagados

  int mejor = -1;
  for (int i = 0; i < BURST; i++)
    if (recibidoP[i] && (mejor < 0 || delays[i] < delays[mejor])) mejor = i;

  if (mejor >= 0) { offsetActual = offsets[mejor]; hayOffset = true; }

  // [exp1] registro de la rafaga (no cambia el algoritmo)
  if (nR < MAX_RESYNC) {
    int np = 0;
    for (int i = 0; i < BURST; i++) if (recibidoP[i]) np++;
    regR[nR].t      = tInicio;
    regR[nR].offset = (mejor >= 0) ? offsets[mejor] : 0;
    regR[nR].rtt    = (mejor >= 0) ? delays[mejor] : -1;
    regR[nR].pongs  = np;
    nR++;
  }
}

// ---------------------------------------------------------------------
// Sensor [exp1]
// ---------------------------------------------------------------------

// Reset por hardware. XSHUT no se fuerza a nivel alto (regla 9).
void resetSensor() {
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
  if (nH >= MAX_EVENTOS) { nHFuera++; return; }
  EvHub &e = evH[nH++];
  e.tDet = eT; e.offset = eOffset; e.ciclo = eCiclo; e.cue = eCue;
  memcpy(e.dt, eDt, sizeof(eDt)); memcpy(e.d, eD, sizeof(eD));
  memcpy(e.s, eS, sizeof(eS));    memcpy(e.m, eM, sizeof(eM));
}

void procesarMuestra(int64_t t, uint16_t d, uint8_t s, uint8_t m) {
  nMuestras++;
  Muestra &w = anillo[(nMuestras - 1) % (PRE + 1)];
  w.t = t; w.d = d; w.s = s; w.m = m;

  if (postFaltan > 0) {                        // completar la traza del evento abierto
    eDt[eIdx] = (int32_t)(t - eT); eD[eIdx] = d; eS[eIdx] = s; eM[eIdx] = m;
    eIdx++;
    if (--postFaltan == 0) cerrarEvento();
  }

  bool blanco = (s == 0 && d < UMBRAL_MM);
  if (blanco && libres >= N_LIBRE && postFaltan == 0 && nMuestras > PRE) {
    eT = t;
    for (int k = 0; k <= PRE; k++) {           // PRE muestras previas + la deteccion
      Muestra &a = anillo[(nMuestras - 1 - PRE + k) % (PRE + 1)];
      eDt[k] = (int32_t)(a.t - eT); eD[k] = a.d; eS[k] = a.s; eM[k] = a.m;
    }
    eIdx = PRE + 1;
    postFaltan = POST;
    eOffset = offsetActual; eCiclo = cicloAct; eCue = cueAct;
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
    if (n - nIsrPrevio > 1) m |= M_DOBLE;      // se perdio una lectura entre dos flancos
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

// [exp1] mueve las detecciones del nodo de la cola del callback al arreglo
void vaciarCola() {
  while (colaOut != colaIn) {
    if (nN < MAX_EVENTOS) {
      memcpy(&evN[nN].tr, &colaTr[colaOut], sizeof(Traza));
      evN[nN].tRx = colaRx[colaOut];
      nN++;
    } else {
      nNFuera++;
    }
    colaOut = (colaOut + 1) % COLA_RX;
  }
}

// ---------------------------------------------------------------------
// Impresion diferida (regla 5)
// ---------------------------------------------------------------------

// Linea con checksum XOR "*HH" (mismo formato que el Exp. 0b y corrida_cielo)
void lineaConChecksum(const char *s) {
  uint8_t x = 0;
  for (const char *p = s; *p; p++) x ^= (uint8_t)*p;
  Serial.printf("%s*%02X\n", s, x);
}

// Agrega ",dt0;dt1;...,d0;...,s0;...,m0;..." a la linea
int agregarTraza(char *b, int n, int cap, const int32_t *dt, const uint16_t *d,
                 const uint8_t *s, const uint8_t *m) {
  for (int k = 0; k < TRAZA; k++) n += snprintf(b + n, cap - n, "%c%ld", k ? ';' : ',', (long)dt[k]);
  for (int k = 0; k < TRAZA; k++) n += snprintf(b + n, cap - n, "%c%u",  k ? ';' : ',', d[k]);
  for (int k = 0; k < TRAZA; k++) n += snprintf(b + n, cap - n, "%c%u",  k ? ';' : ',', s[k]);
  for (int k = 0; k < TRAZA; k++) n += snprintf(b + n, cap - n, "%c%u",  k ? ';' : ',', m[k]);
  return n;
}

void imprimirResultados() {
  char l[640];
  Serial.printf("# exp1 hub | IMP %d ms | umbral %d mm | N_LIBRE %d | traza %d+1+%d | ciclos %d de %d ms (LED %d ms)\n",
                IMP_MS, UMBRAL_MM, N_LIBRE, PRE, POST, N_CICLOS, CUE_PERIODO_MS, CUE_ENCENDIDO_MS);

  Serial.println("# R,k,t_hub_us,offset_us,rtt_us,pongs");
  for (int k = 0; k < nR; k++) {
    snprintf(l, sizeof(l), "R,%d,%lld,%lld,%lld,%u", k, (long long)regR[k].t,
             (long long)regR[k].offset, (long long)regR[k].rtt, regR[k].pongs);
    lineaConChecksum(l);
  }

  Serial.println("# H,k,ciclo,cue,t_det_us,offset_us,dt_us(16),dist_mm(16),status(16),marca(16)  [deteccion = indice 10]");
  for (int k = 0; k < nH; k++) {
    EvHub &e = evH[k];
    int n = snprintf(l, sizeof(l), "H,%d,%d,%u,%lld,%lld", k, e.ciclo, e.cue,
                     (long long)e.tDet, (long long)e.offset);
    agregarTraza(l, n, sizeof(l), e.dt, e.d, e.s, e.m);
    lineaConChecksum(l);
  }

  Serial.println("# N,k,seq_nodo,t_det_nodo_us,t_rx_hub_us,muestras_nodo,rescates_nodo,dobles_nodo,dt_us(16),dist_mm(16),status(16),marca(16)");
  for (int k = 0; k < nN; k++) {
    Traza &r = evN[k].tr;
    int n = snprintf(l, sizeof(l), "N,%d,%lu,%lld,%lld,%lu,%u,%u", k, (unsigned long)r.seq,
                     (long long)r.tDet, (long long)evN[k].tRx, (unsigned long)r.nMuestras,
                     r.nRescates, r.nDobles);
    agregarTraza(l, n, sizeof(l), r.dt, r.dist, r.st, r.marca);
    lineaConChecksum(l);
  }

  Serial.println("# S,muestras_hub,rescates,dobles,tardias,det_hub,det_nodo,det_hub_fuera,det_nodo_fuera,cola_llena,rafagas");
  snprintf(l, sizeof(l), "S,%lu,%lu,%lu,%lu,%d,%d,%d,%d,%lu,%d",
           (unsigned long)nMuestras, (unsigned long)nRescates, (unsigned long)nDobles,
           (unsigned long)nTardias, nH, nN, nHFuera, nNFuera, (unsigned long)nColaLlena, nR);
  lineaConChecksum(l);
  Serial.println("# fin");
}

// ---------------------------------------------------------------------

void setup() {
  Serial.begin(921600);
  delay(1000);
  pinMode(PIN_LED, OUTPUT); pinMode(PIN_CUE, OUTPUT);
  digitalWrite(PIN_LED, LOW); digitalWrite(PIN_CUE, LOW);

  // [exp1] almacenamiento de la corrida, una sola vez y antes de WiFi
  evH = (EvHub *)malloc(sizeof(EvHub) * MAX_EVENTOS);
  evN = (EvNodo *)malloc(sizeof(EvNodo) * MAX_EVENTOS);
  if (!evH || !evN) {
    Serial.println("# FALLO malloc de los arreglos de eventos");
    Serial.println("# fin");
    fase = TERMINADO;
    return;
  }

  // [exp1] sensor: se arranca aqui; la ISR se engancha despues del handshake
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);                       // regla 10 (provisional)
  pinMode(PIN_INT, INPUT_PULLUP);
  resetSensor();
  sensor.setTimeout(500);
  if (!sensor.init()) {                        // regla 8: una sola vez
    Serial.println("# FALLO init() del sensor del hub. Revisa VIN/GND, SDA 21, SCL 22, XSHUT 17.");
    Serial.println("# fin");
    fase = TERMINADO;
    return;
  }
  sensor.setDistanceMode(VL53L1X::Short);
  sensor.setMeasurementTimingBudget(BUDGET_US);
  sensor.startContinuous(IMP_MS);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) { Serial.println("error init"); fase = TERMINADO; return; }
  esp_now_register_recv_cb(onRecv);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, macNodo, 6);
  peer.channel = 0; peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) { Serial.println("error peer"); fase = TERMINADO; return; }

  Serial.printf("# exp1 hub | STA %s | sensor OK | primer resync...\n", WiFi.macAddress().c_str());
}

void loop() {
  if (fase == TERMINADO) return;

  // --- 1. Primer resync + handshake de arranque (Exp. 0) ---
  if (fase == ARRANQUE) {
    ultimoResync = esp_timer_get_time();
    hacerResync();
    if (!hayOffset) return;                    // no hubo respuesta, reintenta

    // avisa al nodo que enganche su ISR y empiece a detectar
    Mensaje m = {}; m.tipo = 4; m.seq = 0;
    esp_now_send(macNodo, (uint8_t *)&m, sizeof(m));
    delay(150);                                // deja que el nodo se prepare

    Serial.println("# handshake listo; LED rapido = preparate, LED 1 s = cruza");
    Serial.flush();

    // [exp1] regla 7: ISR despues de startContinuous() y con INT limpio
    limpiarIntPendiente();
    nueva = false;
    nIsrPrevio = nIsr;
    attachInterrupt(digitalPinToInterrupt(PIN_INT), isrINT, FALLING);
    tUltima = esp_timer_get_time();
    tCue0 = millis() + PREVIO_MS;
    fase = CORRIENDO;
    return;
  }

  atenderSensor();
  vaciarCola();

  uint32_t ahora = millis();

  if (fase == CORRIENDO) {
    int32_t rel = (int32_t)(ahora - tCue0);
    if (rel < 0) {                             // parpadeo rapido: preparate
      cicloAct = -1;
      cueAct = 0;
      uint8_t p = (ahora / 125) % 2;
      digitalWrite(PIN_LED, p); digitalWrite(PIN_CUE, p);
      return;
    }
    int32_t ciclo = rel / CUE_PERIODO_MS;
    int32_t enCiclo = rel % CUE_PERIODO_MS;
    if (ciclo >= N_CICLOS) {
      digitalWrite(PIN_LED, LOW); digitalWrite(PIN_CUE, LOW);
      cueAct = 0;
      tFin = ahora;
      fase = COLA;
      return;
    }
    cicloAct = (int16_t)ciclo;
    cueAct = enCiclo < CUE_ENCENDIDO_MS;
    digitalWrite(PIN_LED, cueAct); digitalWrite(PIN_CUE, cueAct);

    // --- 2. Resync periodico, solo con el LED apagado y el cruce terminado ---
    if (esp_timer_get_time() - ultimoResync >= PERIODO_RESYNC_US &&
        enCiclo >= RESYNC_DESDE_MS && enCiclo < RESYNC_HASTA_MS) {
      ultimoResync = esp_timer_get_time();
      hacerResync();
    }
    return;
  }

  if (fase == COLA && ahora - tFin >= COLA_MS) {
    Mensaje m = {}; m.tipo = 6; m.seq = 0;     // [exp1] el nodo imprime su respaldo
    esp_now_send(macNodo, (uint8_t *)&m, sizeof(m));
    detachInterrupt(digitalPinToInterrupt(PIN_INT));
    sensor.stopContinuous();
    delay(200);
    vaciarCola();
    imprimirResultados();
    fase = TERMINADO;
  }
}
