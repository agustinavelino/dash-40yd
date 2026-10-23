#include <WiFi.h>
#include <esp_now.h>

uint8_t macB[] = {
  0xB0, 0xCB, 0xD8, 0xCA, 0x11, 0xF0
};  // <-- MAC del nodo B

#define BURST              7        // pings por rafaga
#define ESPACIADO_MS       20       // separacion entre pings
#define PERIODO_RESYNC_US  30000000  // 1 s para probar (final: 30000000)

typedef struct __attribute__((packed)) {
  uint8_t  tipo;
  uint32_t seq;
  int64_t  t1;
  int64_t  t2;
  int64_t  t3;
} Mensaje;

volatile int64_t  offsets[BURST];
volatile int64_t  delays[BURST];
volatile bool     recibido[BURST];
volatile uint32_t seqBase = 1;

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  int64_t t4 = esp_timer_get_time();          // primera instruccion
  if (len != sizeof(Mensaje)) return;
  Mensaje m;
  memcpy(&m, data, sizeof(m));
  if (m.tipo != 2) return;

  uint32_t idx = m.seq - seqBase;
  if (idx >= BURST) return;                   // fuera de la rafaga actual

  offsets[idx]  = ((m.t2 - m.t1) + (m.t3 - t4)) / 2;
  delays[idx]   = (t4 - m.t1) - (m.t3 - m.t2);
  recibido[idx] = true;
}

void setup() {
  Serial.begin(921600);
  delay(1000);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) { Serial.println("error init"); return; }
  esp_now_register_recv_cb(onRecv);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, macB, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) { Serial.println("error peer"); return; }

  Serial.println("ronda,off_filtrado,delay_min,off_crudo,delay_crudo,n_ok");
}

int64_t  ultimoResync = 0;
uint32_t seqGlobal = 0;
uint32_t ronda = 0;

void loop() {
  if (esp_timer_get_time() - ultimoResync < PERIODO_RESYNC_US) return;
  ultimoResync = esp_timer_get_time();
  ronda++;

  for (int i = 0; i < BURST; i++) recibido[i] = false;
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

  int mejor = -1, crudo = -1, n = 0;
  for (int i = 0; i < BURST; i++) {
    if (!recibido[i]) continue;
    n++;
    if (crudo < 0) crudo = i;                  // el primero que llego
    if (mejor < 0 || delays[i] < delays[mejor]) mejor = i;
  }

  if (mejor < 0) { Serial.print(ronda); Serial.println(",SIN_RESPUESTA"); return; }

  Serial.print(ronda);                       Serial.print(",");
  Serial.print((long long)offsets[mejor]);   Serial.print(",");
  Serial.print((long long)delays[mejor]);    Serial.print(",");
  Serial.print((long long)offsets[crudo]);   Serial.print(",");
  Serial.print((long long)delays[crudo]);    Serial.print(",");
  Serial.println(n);
}