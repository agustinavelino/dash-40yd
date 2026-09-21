#include <WiFi.h>
#include <esp_now.h>

uint8_t macA[] = {
  0xA4, 0xF0, 0x0F, 0x64, 0xAC, 0x68
};  // <-- MAC del nodo A

typedef struct __attribute__((packed)) {
  uint8_t  tipo;
  uint32_t seq;
  int64_t  t1;
  int64_t  t2;
  int64_t  t3;
} Mensaje;

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  int64_t t2 = esp_timer_get_time();          // primera instruccion
  if (len != sizeof(Mensaje)) return;
  Mensaje m;
  memcpy(&m, data, sizeof(m));
  if (m.tipo != 1) return;

  m.tipo = 2;
  m.t2   = t2;
  m.t3   = esp_timer_get_time();
  esp_now_send(macA, (uint8_t *)&m, sizeof(m));
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) { Serial.println("error init"); return; }
  esp_now_register_recv_cb(onRecv);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, macA, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) { Serial.println("error peer"); return; }

  Serial.println("B listo");
}

void loop() {}