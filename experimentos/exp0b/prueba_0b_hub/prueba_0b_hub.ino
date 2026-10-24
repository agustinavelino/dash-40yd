// =====================================================================
// DASH Timing System - Experimento 0b - HUB (nodo de 40 yd)
//
// Base: experimentos/exp0/prueba_0_A/prueba_0_A.ino, SIN tocar la ISR,
// el algoritmo de resync ni la formula resultado = tHub - (tNodo - offset).
// Todo lo agregado lleva la marca [0b]. Ver docs/experimento0b_contexto.md.
//
// Que agrega:
//   - SoftAP en canal fijo + servidor web asincrono + WebSocket (/ws)
//   - JSON por cada evento procesado, enviado desde loop() (nunca en la
//     ISR ni en el callback de ESP-NOW)
//   - CSV extendido: seq,resultado_us,offset_us,rtt_us,pongs_recibidos,clientes_ws
//   - Al final, los round-trips de TODAS las rafagas (lineas "R,...")
//   - Cada linea de datos con checksum "*HH" y la tabla impresa dos veces
//   - Marcador de fin: la linea exacta "# fin" (usar --fin "^# fin$";
//     "^# fin" tambien atrapa "# fin de espera")
//
// Escenarios (docs/experimento0b_contexto.md, 3.1):
//   C0: compilar con -DSOFTAP_ACTIVO=0 (radio en el mismo canal, sin AP)
//   S1, S2, S3: SOFTAP_ACTIVO=1 (default). Lo que cambia son los clientes.
//   Con SoftAP, el hub espera ESPERA_INICIO_S (o el boton "Iniciar" de la
//   pagina) antes del primer resync, para que los telefonos se reconecten
//   despues del reinicio que provoca abrir el puerto serial.
//
// Compilar con particion grande (la pagina pesada no cabe en 1.2 MB):
//   ARD_OPCIONES=PartitionScheme=huge_app tools/ard.sh upload <sketch> COMn
//   ARD_OPCIONES=PartitionScheme=huge_app ARD_FLAGS=-DSOFTAP_ACTIVO=0 ...   (C0)
//
// Librerias: ESP Async WebServer 3.12.1 + Async TCP 3.5.0 (ESP32Async)
// =====================================================================

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>                          // [0b] esp_wifi_set_channel
#include <AsyncTCP.h>                          // [0b]
#include <ESPAsyncWebServer.h>                 // [0b]
#include "pagina_gz.h"                         // [0b] generada por generar_pagina.py

// [0b] --- configuracion del experimento ---
#ifndef SOFTAP_ACTIVO
#define SOFTAP_ACTIVO 1
#endif
#ifndef ESPERA_INICIO_S
#define ESPERA_INICIO_S 60                     // tiempo para que se reconecten los telefonos
#endif
#define CANAL       1                          // mismo canal en hub y nodo (trampa 1)
#define AP_SSID     "DASH-0b"
#define AP_PASS     "dash0b2026"
#define MAX_RAFAGAS 64                         // ~5 rafagas por corrida; sobra

uint8_t macB[] = {0xB0, 0xCB, 0xD8, 0xCA, 0x11, 0xF0};  // MAC STA del nodo

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
  int64_t  t1;   // en tipo 3 lleva el timestamp del evento en el nodo
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

// [0b] --- registro extra ---
int64_t  rttActual = 0;                        // round-trip del ping elegido en el resync vigente
int      pongsActual = 0;                      // cuantos de los 7 pongs llegaron
int64_t  rttLog[N_EVENTOS + 2];
int8_t   pongsLog[N_EVENTOS + 2];
uint8_t  clientesLog[N_EVENTOS + 2];
bool     enviadoWS[N_EVENTOS + 2];
int64_t  rttRafagas[MAX_RAFAGAS][BURST];       // -1 = ese pong no llego
int      nRafagas = 0;

// [0b] --- servidor ---
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
volatile bool iniciarYa = false;               // boton "Iniciar" de la pagina
bool     esperaTerminada = !SOFTAP_ACTIVO;
uint32_t tArranqueMs = 0, ultimaLimpiezaMs = 0;

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
  else if (m.tipo == 3) {                      // el nodo reporta su evento
    if (m.seq >= 1 && m.seq <= N_EVENTOS) {
      tEventoB[m.seq] = m.t1;
      tengoB[m.seq]   = true;
    }
  }
}

// [0b] Callback del WebSocket: corre en la tarea de AsyncTCP. Solo levanta
// una bandera; no toca nada de la medicion.
void onWs(AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType tipo,
          void *arg, uint8_t *data, size_t len) {
  if (tipo == WS_EVT_DATA && len >= 7 && memcmp(data, "iniciar", 7) == 0) iniciarYa = true;
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

  // [0b] solo registro: no cambia la seleccion de arriba
  int n = 0;
  for (int i = 0; i < BURST; i++) if (recibidoP[i]) n++;
  if (mejor >= 0) { rttActual = delays[mejor]; pongsActual = n; }
  if (nRafagas < MAX_RAFAGAS) {
    for (int i = 0; i < BURST; i++) rttRafagas[nRafagas][i] = recibidoP[i] ? delays[i] : -1;
    nRafagas++;
  }
}

void setup() {
  Serial.begin(921600);
  delay(1000);

  pinMode(PIN_PULSO, OUTPUT);
  digitalWrite(PIN_PULSO, LOW);
  pinMode(PIN_EVENTO, INPUT);
  // OJO: la interrupcion NO se adjunta aqui. Se adjunta despues del
  // handshake, para no contar flancos espurios durante el arranque.

  // [0b] Radio: AP+STA con SoftAP en CANAL, o solo STA forzado a CANAL (C0).
  // En ambos casos el nodo esta fijo en CANAL, asi C0 solo difiere en el AP.
#if SOFTAP_ACTIVO
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS, CANAL, 0, 4);
#else
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(CANAL, WIFI_SECOND_CHAN_NONE);
#endif

  if (esp_now_init() != ESP_OK) { Serial.println("error init"); return; }
  esp_now_register_recv_cb(onRecv);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, macB, 6);
  peer.channel = CANAL;                        // [0b] canal explicito (antes 0)
  peer.ifidx   = WIFI_IF_STA;                  // [0b] trampa 2: el nodo conoce la MAC STA
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) { Serial.println("error peer"); return; }

  // [0b] servidor web. La pagina se sirve comprimida y con no-store, para
  // que cada recarga del modo estres vuelva a bajar todo (es la carga).
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    AsyncWebServerResponse *r = req->beginResponse(200, "text/html", PAGINA_GZ, PAGINA_GZ_LEN);
    r->addHeader("Content-Encoding", "gzip");
    r->addHeader("Cache-Control", "no-store");
    req->send(r);
  });
  ws.onEvent(onWs);
  server.addHandler(&ws);
#if SOFTAP_ACTIVO
  server.begin();
#endif

  for (int i = 0; i <= N_EVENTOS + 1; i++) {
    tengoA[i] = false; tengoB[i] = false; enviadoWS[i] = false;   // [0b] enviadoWS
  }

  // [0b] datos de arranque para verificar canal y MACs (trampas 1 y 2).
  // Se imprimen ANTES de la corrida, asi que no meten jitter.
  uint8_t canal; wifi_second_chan_t sec;
  esp_wifi_get_channel(&canal, &sec);
  Serial.printf("# exp0b hub | SOFTAP_ACTIVO=%d | canal %u | STA %s | AP %s | pagina %u B gz\n",
                SOFTAP_ACTIVO, canal, WiFi.macAddress().c_str(),
                SOFTAP_ACTIVO ? WiFi.softAPmacAddress().c_str() : "-", (unsigned)PAGINA_GZ_LEN);
  if (SOFTAP_ACTIVO)
    Serial.printf("# SoftAP %s / %s -> http://%s  | espera hasta %d s o boton Iniciar\n",
                  AP_SSID, AP_PASS, WiFi.softAPIP().toString().c_str(), ESPERA_INICIO_S);
  tArranqueMs = millis();
  Serial.println("# arrancando, primer resync...");
}

// [0b] Imprime una linea con checksum al final ("*HH", XOR de los bytes,
// como NMEA). En S1 se corrompieron bytes del serial con el SoftAP activo
// y una linea danada (rtt 2981 -> 981) paso como valida; con el checksum
// resumen_corrida.py la descarta en vez de aceptarla.
void lineaConChecksum(const char *s) {
  uint8_t x = 0;
  for (const char *p = s; *p; p++) x ^= (uint8_t)*p;
  Serial.printf("%s*%02X\n", s, x);
}

void imprimirResultados() {
  delay(500);                                  // deja llegar reportes rezagados
  char l[96];
  // [0b] La tabla se imprime DOS veces: una linea corrupta en una copia se
  // recupera de la otra. Cuesta ~0.5 s despues de la corrida; no toca la medicion.
  for (int copia = 1; copia <= 2; copia++) {
    Serial.printf("# copia %d\n", copia);
    Serial.println("seq,resultado_us,offset_us,rtt_us,pongs_recibidos,clientes_ws");   // [0b] columnas extra
    for (uint32_t i = 1; i <= N_EVENTOS; i++) {
      if (!tengoA[i] || !tengoB[i]) { snprintf(l, sizeof(l), "%lu,FALTA,", (unsigned long)i); lineaConChecksum(l); continue; }
      int64_t res = tEventoALog[i] - (tEventoB[i] - offsetLog[i]);
      snprintf(l, sizeof(l), "%lu,%lld,%lld,%lld,%d,%u", (unsigned long)i, (long long)res,
               (long long)offsetLog[i], (long long)rttLog[i], pongsLog[i], clientesLog[i]);
      lineaConChecksum(l);
    }
    // [0b] todos los round-trips, para ver si el WiFi sube el piso F.
    // Empiezan con "R" para que resumen_corrida.py no los confunda con eventos.
    Serial.println("# rafagas: R,rafaga,ping,rtt_us (-1 = no llego)");
    for (int r = 0; r < nRafagas; r++)
      for (int i = 0; i < BURST; i++) {
        snprintf(l, sizeof(l), "R,%d,%d,%lld", r + 1, i + 1, (long long)rttRafagas[r][i]);
        lineaConChecksum(l);
      }
  }
  Serial.printf("# estaciones_ap_final %d | clientes_ws_final %u\n",
                SOFTAP_ACTIVO ? WiFi.softAPgetStationNum() : 0, (unsigned)ws.count());
  Serial.println("# fin");
}

// [0b] Manda por WebSocket los eventos ya completos (hub y nodo).
// Corre en loop(): nunca en la ISR ni en el callback de ESP-NOW.
void enviarPendientesWS() {
  uint32_t hasta = seqA;
  if (hasta > N_EVENTOS) hasta = N_EVENTOS;
  for (uint32_t s = 1; s <= hasta; s++) {
    if (enviadoWS[s] || !tengoA[s] || !tengoB[s]) continue;
    enviadoWS[s] = true;
    if (ws.count() == 0) continue;
    int64_t res = tEventoALog[s] - (tEventoB[s] - offsetLog[s]);
    char json[112];
    snprintf(json, sizeof(json), "{\"seq\":%lu,\"res_us\":%lld,\"off_us\":%lld,\"rtt_us\":%lld,\"pongs\":%d}",
             (unsigned long)s, (long long)res, (long long)offsetLog[s],
             (long long)rttLog[s], pongsLog[s]);
    ws.textAll(json);
  }
}

void loop() {
  if (terminado) return;

  // [0b] limpieza de clientes WebSocket caidos (las recargas de S3 los dejan)
  if (millis() - ultimaLimpiezaMs > 1000) { ultimaLimpiezaMs = millis(); ws.cleanupClients(); }

  // [0b] --- 0. Espera a que se conecten los telefonos (solo con SoftAP) ---
  if (!esperaTerminada) {
    if (!iniciarYa && millis() - tArranqueMs < ESPERA_INICIO_S * 1000UL) return;
    esperaTerminada = true;
    Serial.printf("# fin de espera (%s) | estaciones_ap %d | clientes_ws %u\n",
                  iniciarYa ? "boton" : "tiempo", WiFi.softAPgetStationNum(), (unsigned)ws.count());
  }

  // --- 1. Primer resync + handshake de arranque ---
  if (!hayOffset) {
    ultimoResync = esp_timer_get_time();
    hacerResync();
    if (!hayOffset) return;                    // no hubo respuesta, reintenta

    // avisa al nodo que ponga su contador en cero y empiece a escuchar
    Mensaje m = {}; m.tipo = 4; m.seq = 0;
    esp_now_send(macB, (uint8_t *)&m, sizeof(m));
    delay(150);                                // deja que el nodo se prepare

    // ahora si, el hub arranca sus propios contadores desde cero
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
      rttLog[s]      = rttActual;               // [0b]
      pongsLog[s]    = pongsActual;             // [0b]
      clientesLog[s] = ws.count();              // [0b]
    }
    if (s >= N_EVENTOS) { imprimirResultados(); terminado = true; return; }
  }

  enviarPendientesWS();                        // [0b]

  // --- 4. Generar el siguiente pulso ---
  if (esp_timer_get_time() - ultimoPulso >= PERIODO_PULSO_US) {
    ultimoPulso = esp_timer_get_time();
    digitalWrite(PIN_PULSO, HIGH);
    delayMicroseconds(200);
    digitalWrite(PIN_PULSO, LOW);
  }
}
