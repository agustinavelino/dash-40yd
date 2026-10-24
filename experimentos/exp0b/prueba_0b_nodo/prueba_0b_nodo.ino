// =====================================================================
// DASH Timing System - Experimento 0b - NODO (puerta de 0/10/20 yd)
//
// Base: experimentos/exp0/prueba_0_B/prueba_0_B.ino. Unico cambio: el radio
// queda fijo en el canal del SoftAP del hub (trampa 1 del plan del 0b: si
// el nodo esta en otro canal, ESP-NOW deja de funcionar en silencio y el
// handshake nunca completa). Lo agregado lleva la marca [0b].
// El mismo binario sirve para los cuatro escenarios (C0, S1, S2, S3).
// =====================================================================

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>                          // [0b] esp_wifi_set_channel

#define CANAL 1                                // [0b] debe coincidir con el hub

uint8_t macA[] = {0xA4, 0xF0, 0x0F, 0x64, 0xAC, 0x68};  // MAC STA del hub (A en el Exp. 0)

#define PIN_EVENTO 16
#define LOCKOUT_US 50000LL

typedef struct __attribute__((packed)) {
  uint8_t  tipo;
  uint32_t seq;
  int64_t  t1, t2, t3;
} Mensaje;

volatile int64_t  tEventoB = 0, ultimoFlanco = 0;
volatile uint32_t seqB = 0;
volatile bool     nuevoEvento = false;
volatile bool     pedidoArranque = false;      // bandera del handshake
bool              interrupcionLista = false;

// ISR: identica a la de A. Solo captura y sale.
void IRAM_ATTR isrEvento() {
  int64_t t = esp_timer_get_time();
  if (t - ultimoFlanco < LOCKOUT_US) return;
  ultimoFlanco = t;
  seqB++;
  tEventoB = t;
  nuevoEvento = true;
}

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  int64_t t2 = esp_timer_get_time();           // PRIMERA instruccion
  if (len != sizeof(Mensaje)) return;
  Mensaje m;
  memcpy(&m, data, sizeof(m));                 // hay que llenar m ANTES de usarlo

  if (m.tipo == 4) {                           // handshake: A dice "arranca"
    pedidoArranque = true;
    return;
  }

  if (m.tipo != 1) return;                     // solo responde pings
  m.tipo = 2;
  m.t2   = t2;
  m.t3   = esp_timer_get_time();
  esp_now_send(macA, (uint8_t *)&m, sizeof(m));
}

void setup() {
  Serial.begin(921600);
  delay(1000);

  pinMode(PIN_EVENTO, INPUT);
  // La interrupcion NO se adjunta aqui: se espera al handshake de A.

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(CANAL, WIFI_SECOND_CHAN_NONE);   // [0b] fijo al canal del SoftAP
  if (esp_now_init() != ESP_OK) { Serial.println("error init"); return; }
  esp_now_register_recv_cb(onRecv);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, macA, 6);
  peer.channel = CANAL; peer.encrypt = false;  // [0b] canal explicito (antes 0)
  if (esp_now_add_peer(&peer) != ESP_OK) { Serial.println("error peer"); return; }

  // [0b] verificar que el canal quedo fijo (se imprime antes de la corrida)
  uint8_t canal; wifi_second_chan_t sec;
  esp_wifi_get_channel(&canal, &sec);
  Serial.printf("# exp0b nodo | canal %u | STA %s\n", canal, WiFi.macAddress().c_str());
  Serial.println("B listo, esperando handshake");
}

void loop() {
  // Adjuntar la interrupcion aqui (no dentro del callback de ESP-NOW)
  if (pedidoArranque && !interrupcionLista) {
    pedidoArranque = false;
    seqB = 0;
    ultimoFlanco = 0;
    attachInterrupt(digitalPinToInterrupt(PIN_EVENTO), isrEvento, RISING);
    interrupcionLista = true;
    Serial.println("# handshake recibido, contador en cero");
  }

  if (!nuevoEvento) return;
  nuevoEvento = false;

  Mensaje m = {};
  m.tipo = 3;
  m.seq  = seqB;
  m.t1   = tEventoB;
  esp_now_send(macA, (uint8_t *)&m, sizeof(m));
}