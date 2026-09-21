#include <WiFi.h>
#include <esp_now.h>

uint8_t macB[] = {0xB0, 0xCB, 0xD8, 0xCA, 0x11, 0xF0};  // MAC del OTRO nodo (B)

#define PIN_PULSO   4      // salida: genera el pulso de prueba
#define PIN_EVENTO  16     // entrada: recibe el pulso (loopback)
#define BURST       7      // pings por rafaga de resync
#define ESPACIADO_MS 20    // separacion entre pings de la rafaga
#define PERIODO_RESYNC_US 30000000LL   // 30 s
#define PERIODO_PULSO_US    500000LL   // 500 ms
#define LOCKOUT_US           50000LL   // ignora flancos dentro de 50 ms
#define N_EVENTOS   200

// tipo: 1=ping  2=pong  3=reporte de evento  4=handshake de arranque
typedef struct __attribute__((packed)) {
  uint8_t  tipo;
  uint32_t seq;
  int64_t  t1;   // en tipo 3 lleva el timestamp del evento en B
  int64_t  t2;
  int64_t  t3;
} Mensaje;

// --- estado del resync ---
volatile int64_t  offsets[BURST], delays[BURST];
volatile bool     recibidoP[BURST];
volatile uint32_t seqBase = 1;
volatile int64_t  offsetActual = 0;
volatile bool     hayOffset = false;

// --- estado de eventos ---
volatile int64_t  tEventoA = 0;
volatile uint32_t seqA = 0;
volatile bool     nuevoEventoA = false;
volatile int64_t  ultimoFlanco = 0;

// --- almacenamiento de la corrida ---
int64_t tEventoALog[N_EVENTOS + 2];
int64_t tEventoB[N_EVENTOS + 2];
int64_t offsetLog[N_EVENTOS + 2];
bool    tengoA[N_EVENTOS + 2];
bool    tengoB[N_EVENTOS + 2];

// ISR: SOLO captura el timestamp y sale. Nada de I2C, serial ni radio aqui.
void IRAM_ATTR isrEvento() {
  int64_t t = esp_timer_get_time();
  if (t - ultimoFlanco < LOCKOUT_US) return;   // rebote o flanco espurio
  ultimoFlanco = t;
  seqA++;
  tEventoA = t;
  nuevoEventoA = true;
}

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  int64_t t4 = esp_timer_get_time();           // PRIMERA instruccion
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
  else if (m.tipo == 3) {                      // B reporta su evento
    if (m.seq >= 1 && m.seq <= N_EVENTOS) {
      tEventoB[m.seq] = m.t1;
      tengoB[m.seq]   = true;
    }
  }
}

uint32_t seqGlobal = 0;
int64_t  ultimoResync = 0, ultimoPulso = 0;
bool     terminado = false;

// Rafaga de BURST pings; se queda con el de menor round-trip.
void hacerResync() {
  for (int i = 0; i < BURST; i++) recibidoP[i] = false;
  seqBase = seqGlobal + 1;

  for (int i = 0; i < BURST; i++) {
    seqGlobal++;
    Mensaje m = {};
    m.tipo = 1;
    m.seq  = seqGlobal;
    m.t1   = esp_timer_get_time();
    esp_now_send(macB, (uint8_t *)&m, sizeof(m));
    delay(ESPACIADO_MS);
  }
  delay(60);                                   // margen para rezagados

  int mejor = -1;
  for (int i = 0; i < BURST; i++)
    if (recibidoP[i] && (mejor < 0 || delays[i] < delays[mejor])) mejor = i;

  if (mejor >= 0) { offsetActual = offsets[mejor]; hayOffset = true; }
}

void setup() {
  Serial.begin(921600);
  delay(1000);

  pinMode(PIN_PULSO, OUTPUT);
  digitalWrite(PIN_PULSO, LOW);
  pinMode(PIN_EVENTO, INPUT);
  // OJO: la interrupcion NO se adjunta aqui. Se adjunta despues del
  // handshake, para no contar flancos espurios durante el arranque.

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) { Serial.println("error init"); return; }
  esp_now_register_recv_cb(onRecv);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, macB, 6);
  peer.channel = 0; peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) { Serial.println("error peer"); return; }

  for (int i = 0; i <= N_EVENTOS + 1; i++) { tengoA[i] = false; tengoB[i] = false; }
  Serial.println("# arrancando, primer resync...");
}

void imprimirResultados() {
  delay(500);                                  // deja llegar reportes rezagados
  Serial.println("seq,resultado_us,offset_us");
  for (uint32_t i = 1; i <= N_EVENTOS; i++) {
    if (!tengoA[i] || !tengoB[i]) { Serial.print(i); Serial.println(",FALTA,"); continue; }
    int64_t res = tEventoALog[i] - (tEventoB[i] - offsetLog[i]);
    Serial.print(i);                       Serial.print(",");
    Serial.print((long long)res);          Serial.print(",");
    Serial.println((long long)offsetLog[i]);
  }
  Serial.println("# fin");
}

void loop() {
  if (terminado) return;

  // --- 1. Primer resync + handshake de arranque ---
  if (!hayOffset) {
    ultimoResync = esp_timer_get_time();
    hacerResync();
    if (!hayOffset) return;                    // no hubo respuesta, reintenta

    // avisa a B que ponga su contador en cero y empiece a escuchar
    Mensaje m = {}; m.tipo = 4; m.seq = 0;
    esp_now_send(macB, (uint8_t *)&m, sizeof(m));
    delay(150);                                // deja que B se prepare

    // ahora si, A arranca sus propios contadores desde cero
    seqA = 0;
    ultimoFlanco = 0;
    attachInterrupt(digitalPinToInterrupt(PIN_EVENTO), isrEvento, RISING);
    ultimoPulso = esp_timer_get_time();
    Serial.println("# handshake listo, empiezan los pulsos");
    return;
  }

  // --- 2. Resync periodico ---
  if (esp_timer_get_time() - ultimoResync >= PERIODO_RESYNC_US) {
    ultimoResync = esp_timer_get_time();
    hacerResync();
    return;
  }

  // --- 3. Procesar evento capturado por la ISR ---
  if (nuevoEventoA) {
    nuevoEventoA = false;
    uint32_t s = seqA;
    if (s >= 1 && s <= N_EVENTOS) {
      tEventoALog[s] = tEventoA;
      offsetLog[s]   = offsetActual;
      tengoA[s]      = true;
    }
    if (s >= N_EVENTOS) { imprimirResultados(); terminado = true; return; }
  }

  // --- 4. Generar el siguiente pulso ---
  if (esp_timer_get_time() - ultimoPulso >= PERIODO_PULSO_US) {
    ultimoPulso = esp_timer_get_time();
    digitalWrite(PIN_PULSO, HIGH);
    delayMicroseconds(200);
    digitalWrite(PIN_PULSO, LOW);
  }
}