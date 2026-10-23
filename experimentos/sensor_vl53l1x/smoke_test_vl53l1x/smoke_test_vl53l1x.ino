// =====================================================================
// DASH Timing System - Smoke test del VL53L1X
// Paso previo al Experimento 1
//
// Que hace:
//   1. Escanea el bus I2C y reporta las direcciones encontradas
//   2. Inicializa el sensor (init() verifica el Model ID: si pasa, ES un
//      VL53L1X y no un VL53L0X)
//   3. Configura modo corto + timing budget de 20 ms + continuo
//   4. Adjunta una ISR al pin INT que captura el timestamp como PRIMERA
//      instruccion, igual que en el Experimento 0
//   5. Imprime CSV: seq, timestamp, periodo entre INTs, distancia, estatus
//   6. Cada 100 muestras imprime estadisticas del periodo
//
// Libreria: "VL53L1X" de Pololu (Gestor de Librerias del IDE de Arduino)
// =====================================================================
 
#include <Wire.h>
#include <VL53L1X.h>
 
#define PIN_SDA     21
#define PIN_SCL     22
#define PIN_INT     16       // GPIO1 del modulo -> GPIO16 del ESP32
 
#define BUDGET_US   20000    // timing budget: 20 ms (minimo en modo corto)
#define PERIODO_MS  20       // inter-measurement period
#define N_BLOQUE    100      // cada cuantas muestras se reportan stats
 
VL53L1X sensor;
 
// --- estado de la ISR ---
volatile int64_t  tINT = 0;
volatile uint32_t seqINT = 0;
volatile bool     nuevaMuestra = false;
 
// --- estadisticas del periodo ---
int64_t tPrevio = 0;
int64_t sumaDelta = 0, sumaDelta2 = 0;
int64_t deltaMin = 0, deltaMax = 0;
uint32_t nBloque = 0;
 
// ISR: SOLO captura el timestamp y sale. Misma disciplina que el Exp. 0.
void IRAM_ATTR isrINT() {
  tINT = esp_timer_get_time();
  seqINT++;
  nuevaMuestra = true;
}
 
void escanearI2C() {
  Serial.println("# --- escaneo I2C ---");
  int encontrados = 0;
  for (uint8_t dir = 1; dir < 127; dir++) {
    Wire.beginTransmission(dir);
    if (Wire.endTransmission() == 0) {
      Serial.print("#   dispositivo en 0x");
      if (dir < 16) Serial.print("0");
      Serial.println(dir, HEX);
      encontrados++;
    }
  }
  if (encontrados == 0)
    Serial.println("#   NADA. Revisa cableado, alimentacion y pull-ups.");
  Serial.print("# total: "); Serial.println(encontrados);
  Serial.println("# esperado: un dispositivo en 0x29");
  Serial.println("# -------------------");
}
 
void setup() {
  Serial.begin(921600);
  delay(1500);
  Serial.println();
  Serial.println("# Smoke test VL53L1X - DASH Timing System");
 
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);        // 100 kHz: conservador para protoboard
 
  escanearI2C();
 
  sensor.setTimeout(500);
  if (!sensor.init()) {
    Serial.println("# FALLO init(). Causas posibles:");
    Serial.println("#   - no es un VL53L1X (init verifica el Model ID)");
    Serial.println("#   - XSHUT esta en bajo");
    Serial.println("#   - alimentacion o cableado I2C");
    while (1) delay(1000);
  }
  Serial.println("# init() OK -> el chip responde y es un VL53L1X");
 
  // Modo corto: el que usa el sistema (seccion 9.3 del Documento Maestro)
  if (!sensor.setDistanceMode(VL53L1X::Short))
    Serial.println("# aviso: setDistanceMode fallo");
 
  if (!sensor.setMeasurementTimingBudget(BUDGET_US))
    Serial.println("# aviso: setMeasurementTimingBudget fallo");
 
  Serial.print("# timing budget solicitado: "); Serial.print(BUDGET_US);
  Serial.print(" us / reportado por el chip: ");
  Serial.print(sensor.getMeasurementTimingBudget());
  Serial.println(" us");
 
  sensor.startContinuous(PERIODO_MS);
 
  // La ISR se adjunta DESPUES de arrancar el continuo, para no contar
  // flancos del arranque. INT es activo en bajo -> FALLING.
  pinMode(PIN_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_INT), isrINT, FALLING);
 
  Serial.println("#");
  Serial.println("seq,t_us,delta_us,dist_mm,estatus");
}
 
void reportarBloque() {
  double media = (double)sumaDelta / nBloque;
  double var   = (double)sumaDelta2 / nBloque - media * media;
  double sd    = var > 0 ? sqrt(var) : 0;
 
  Serial.println("#");
  Serial.print("# --- bloque de "); Serial.print(nBloque);
  Serial.println(" muestras ---");
  Serial.print("#   periodo medio : "); Serial.print(media / 1000.0, 3);
  Serial.println(" ms");
  Serial.print("#   jitter (1 sd) : "); Serial.print(sd, 1);
  Serial.println(" us");
  Serial.print("#   min / max     : "); Serial.print(deltaMin / 1000.0, 3);
  Serial.print(" / "); Serial.print(deltaMax / 1000.0, 3);
  Serial.println(" ms");
  Serial.print("#   >> ANOTA el periodo medio: es la ventana real <<");
  Serial.println();
  Serial.println("#");
 
  sumaDelta = sumaDelta2 = 0;
  deltaMin = deltaMax = 0;
  nBloque = 0;
}
 
void loop() {
  if (!nuevaMuestra) return;
  nuevaMuestra = false;
 
  int64_t  t = tINT;
  uint32_t s = seqINT;
 
  // read() lee la distancia y limpia la interrupcion latcheada.
  // Sin esto, INT no vuelve a bajar nunca.
  uint16_t dist = sensor.read();
  uint8_t  est  = sensor.ranging_data.range_status;
 
  int64_t delta = (tPrevio == 0) ? 0 : (t - tPrevio);
  tPrevio = t;
 
  Serial.print(s);        Serial.print(",");
  Serial.print((long long)t);      Serial.print(",");
  Serial.print((long long)delta);  Serial.print(",");
  Serial.print(dist);     Serial.print(",");
  Serial.println(VL53L1X::rangeStatusToString((VL53L1X::RangeStatus)est));
 
  if (delta > 0) {
    sumaDelta  += delta;
    sumaDelta2 += delta * delta;             // ~4e8 por muestra: cabe en int64
    if (deltaMin == 0 || delta < deltaMin) deltaMin = delta;
    if (delta > deltaMax) deltaMax = delta;
    nBloque++;
    if (nBloque >= N_BLOQUE) reportarBloque();
  }
}