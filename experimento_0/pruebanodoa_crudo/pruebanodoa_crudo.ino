#include <WiFi.h>
#include <esp_now.h>

uint8_t macB[] = {
  0xB0, 0xCB, 0xD8, 0xCA, 0x11, 0xF0
};  // <-- MAC del nodo B

typedef struct __attribute__((packed)) {
  uint8_t  tipo;      // 1 = ping, 2 = pong
  uint32_t seq;
  int64_t  t1;
  int64_t  t2;
  int64_t  t3;
} Mensaje;

volatile int64_t offset = 0;
volatile bool    hayOffsetNuevo = false;
volatile uint32_t seqActual = 0;
volatile int64_t  ultimoT1 = 0;

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  int64_t t4 = esp_timer_get_time();          // primera instruccion
  if (len != sizeof(Mensaje)) return;
  Mensaje m;
  memcpy(&m, data, sizeof(m));
  if (m.tipo != 2) return;
  if (m.seq != seqActual) return;             // respuesta vieja, ignorar

  offset = ((m.t2 - m.t1) + (m.t3 - t4)) / 2;
  hayOffsetNuevo = true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) { Serial.println("error init"); return; }
  esp_now_register_recv_cb(onRecv);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, macB, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) { Serial.println("error peer"); return; }

  Serial.println("seq,offset_us,delta_us");
}

int64_t ultimoResync = 0;
int64_t offsetAnterior = 0;
bool    primero = true;

void loop() {
  int64_t ahora = esp_timer_get_time();

  if (ahora - ultimoResync >= 1000000) {      // 1 s (en el final: 30000000)
    ultimoResync = ahora;
    seqActual++;

    Mensaje m = {};
    m.tipo = 1;
    m.seq  = seqActual;
    m.t1   = esp_timer_get_time();
    ultimoT1 = m.t1;
    esp_now_send(macB, (uint8_t *)&m, sizeof(m));
  }

  if (hayOffsetNuevo) {
    hayOffsetNuevo = false;
    int64_t o = offset;
    int64_t delta = primero ? 0 : (o - offsetAnterior);
    primero = false;
    offsetAnterior = o;

    Serial.print(seqActual); Serial.print(",");
    Serial.print((long long)o); Serial.print(",");
    Serial.println((long long)delta);
  }
}