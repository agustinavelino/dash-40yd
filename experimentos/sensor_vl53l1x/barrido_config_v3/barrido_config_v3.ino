// =====================================================================
// DASH Timing System - Barrido de configuracion del VL53L1X  (v3)
//
// Objetivo: averiguar de que depende el periodo de INT (37.23 ms
// medido en el smoke test) y si se puede bajar.
//
// POR QUE FALLABA LA v2:
//   El pin INT del VL53L1X es activo en bajo y queda LATCHEADO hasta
//   que read() lo limpia. La v2 hacia attachInterrupt(FALLING) en
//   setup(), antes de cualquier startContinuous(). Si el pin ya estaba
//   abajo en ese momento, no habia flanco que detectar, la ISR nunca
//   disparaba, read() nunca se llamaba y la interrupcion nunca se
//   limpiaba. Bloqueo circular -> "sin interrupciones".
//   Ademas la v2 llamaba init() dentro de cada config. init() hace un
//   reset por software del chip; repetirlo a media sesion lo deja en
//   mal estado y provoca el "bus no responde".
//
// ARREGLOS DE LA v3:
//   - attachInterrupt DESPUES de startContinuous, en cada config, con
//     detachInterrupt antes de reconfigurar. Mismo orden que el smoke
//     test, que si funciona.
//   - Se limpia cualquier INT latcheado antes de enganchar la ISR.
//   - Rescate en los bucles de espera: si no llega flanco pero el pin
//     esta abajo, se llama read() para destrabarlo. El barrido se
//     recupera solo.
//   - init() solo una vez, en setup(). Para cambiar config basta
//     stopContinuous -> reconfigurar -> startContinuous.
//   - I2C a 100 kHz (a 400 kHz el bus se caia).
//
// COMO USARLO:
//   1. Desconecta y reconecta el USB antes de correr.
//   2. Blanco FIJO a ~1 m. No moverlo durante la corrida.
//   3. Dura alrededor de un minuto. Cada config imprime al terminar.
//
// Libreria: "VL53L1X" de Pololu
// =====================================================================
 
#include <Wire.h>
#include <VL53L1X.h>
 
#define PIN_SDA     21
#define PIN_SCL     22
#define PIN_INT     16
 
#define N_DESCARTE  8      // muestras de asentamiento que se tiran
#define N_MEDIR     50     // muestras que entran a la estadistica
 
#define MODO_CORTO  0
#define MODO_MEDIO  1
#define MODO_LARGO  2
 
#define REG_MODEL_ID 0x010F     // debe leer 0xEACC en un VL53L1X
 
VL53L1X sensor;
 
volatile int64_t  tINT = 0;
volatile bool     nueva = false;
 
void IRAM_ATTR isrINT() {
  tINT = esp_timer_get_time();
  nueva = true;
}
 
// --- configuraciones (arreglos paralelos) ---
// Regla: imp_ms >= budget_us/1000 SIEMPRE. El chip lo exige; con IMP
// menor deja de generar INT.
 
const uint32_t cfgBudget[] = {
  20000, 20000, 20000, 20000, 20000, 20000,
  25000, 33000, 50000,
  33000, 33000,
  20000
};
 
const uint32_t cfgImp[] = {
  20, 25, 33, 40, 50, 60,
  25, 33, 50,
  33, 33,
  20
};
 
const uint8_t cfgModo[] = {
  MODO_CORTO, MODO_CORTO, MODO_CORTO, MODO_CORTO, MODO_CORTO, MODO_CORTO,
  MODO_CORTO, MODO_CORTO, MODO_CORTO,
  MODO_MEDIO, MODO_LARGO,
  MODO_CORTO
};
 
const char *cfgNota[] = {
  "corto, budget min, IMP 20 (la config actual)",
  "corto, budget min, IMP 25",
  "corto, budget min, IMP 33",
  "corto, budget min, IMP 40 (por encima del piso)",
  "corto, budget min, IMP 50 (por encima del piso)",
  "corto, budget min, IMP 60 (por encima del piso)",
  "corto, budget 25 ms, IMP 25",
  "corto, budget 33 ms, IMP 33",
  "corto, budget 50 ms, IMP 50",
  "MEDIO, budget 33 ms, IMP 33 (diagnostico)",
  "LARGO, budget 33 ms, IMP 33 (diagnostico)",
  "CONTROL: repite la primera"
};
 
const int N_CONFIGS = sizeof(cfgBudget) / sizeof(cfgBudget[0]);
 
// --- resultados de la ultima medicion ---
double   rMedia = 0, rSd = 0;
int64_t  rMin = 0, rMax = 0;
int      rValidas = 0, rTotal = 0, rRescates = 0;
uint32_t rBudgetReal = 0;
bool     rBudgetOk = true;
bool     rBusVivo = true;
 
const char *nombreModo(uint8_t m) {
  if (m == MODO_CORTO) return "corto";
  if (m == MODO_MEDIO) return "medio";
  return "largo";
}
 
bool busResponde() {
  return sensor.readReg16Bit((VL53L1X::regAddr)REG_MODEL_ID) == 0xEACC;
}
 
// Limpia un INT que haya quedado latcheado de una config anterior.
void limpiarIntPendiente() {
  for (int i = 0; i < 5 && digitalRead(PIN_INT) == LOW; i++) {
    sensor.read(false);          // lee sin esperar y limpia la interrupcion
    delay(5);
  }
}
 
// Espera un flanco. Si no llega pero el pin esta abajo, destraba
// llamando read(). Devuelve true si hubo muestra.
bool esperarMuestra(uint32_t limite_ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < limite_ms) {
    if (nueva) { nueva = false; return true; }
    if (digitalRead(PIN_INT) == LOW) {   // flanco perdido: rescate
      sensor.read(false);
      rRescates++;
      delay(2);
    }
  }
  return false;
}
 
void medirConfig(uint32_t budget_us, uint32_t imp_ms, uint8_t modo) {
  rMedia = 0; rSd = 0; rMin = 0; rMax = 0;
  rValidas = 0; rTotal = 0; rRescates = 0; rBudgetReal = 0; rBusVivo = true;
 
  detachInterrupt(digitalPinToInterrupt(PIN_INT));
  sensor.stopContinuous();
  delay(50);
  limpiarIntPendiente();
 
  if (!busResponde()) { rBusVivo = false; return; }
 
  if      (modo == MODO_CORTO) sensor.setDistanceMode(VL53L1X::Short);
  else if (modo == MODO_MEDIO) sensor.setDistanceMode(VL53L1X::Medium);
  else                         sensor.setDistanceMode(VL53L1X::Long);
 
  rBudgetOk   = sensor.setMeasurementTimingBudget(budget_us);
  rBudgetReal = sensor.getMeasurementTimingBudget();
  if (rBudgetReal == 0) { rBusVivo = false; return; }
 
  Serial.print("#   budget pedido "); Serial.print(budget_us);
  Serial.print(" us -> chip reporta "); Serial.print(rBudgetReal);
  Serial.print(" us");
  if (!rBudgetOk) Serial.print("   [RECHAZADO por la libreria]");
  Serial.println();
 
  sensor.startContinuous(imp_ms);
  delay(20);
 
  // La ISR se engancha DESPUES de arrancar el continuo y con el pin
  // ya limpio: asi el primer flanco de bajada si se ve.
  limpiarIntPendiente();
  nueva = false;
  attachInterrupt(digitalPinToInterrupt(PIN_INT), isrINT, FALLING);
 
  // Asentamiento.
  int descartadas = 0;
  while (descartadas < N_DESCARTE) {
    if (!esperarMuestra(1000)) break;
    sensor.read();
    descartadas++;
  }
 
  if (descartadas < N_DESCARTE) {
    Serial.println("#   esta config no produce INT de forma sostenida.");
    return;
  }
 
  // Medicion.
  int64_t prev = 0;
  int n = 0;
  double suma = 0, suma2 = 0;
 
  while (n < N_MEDIR) {
    if (!esperarMuestra(1000)) break;
 
    int64_t t = tINT;
    sensor.read();
    rTotal++;
    if (sensor.ranging_data.range_status == VL53L1X::RangeValid) rValidas++;
 
    if (prev != 0) {
      int64_t d = t - prev;
      suma  += (double)d;
      suma2 += (double)d * (double)d;
      if (rMin == 0 || d < rMin) rMin = d;
      if (d > rMax) rMax = d;
      n++;
    }
    prev = t;
  }
 
  if (n == 0) return;
  rMedia = suma / n;
  double var = suma2 / n - rMedia * rMedia;
  rSd = var > 0 ? sqrt(var) : 0;
}
 
void setup() {
  Serial.begin(921600);
  delay(1500);
  Serial.println();
  Serial.println("# Barrido de configuracion VL53L1X - DASH (v3)");
  Serial.println("# Blanco FIJO a ~1 m. No moverlo durante la corrida.");
  Serial.println();
 
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
 
  pinMode(PIN_INT, INPUT_PULLUP);
 
  sensor.setTimeout(500);
  if (!sensor.init()) {
    Serial.println("# FALLO init() al arrancar.");
    Serial.println("# Desconecta y reconecta el USB: si hubo un crash");
    Serial.println("# previo, el sensor quedo colgado y XSHUT no esta");
    Serial.println("# cableado para resetearlo por software.");
    while (1) delay(1000);
  }
  Serial.println("# init() OK");
 
  Serial.println();
  Serial.println("modo,budget_pedido_us,budget_real_us,imp_ms,"
                 "periodo_ms,jitter_us,min_ms,max_ms,validas,total,rescates,nota");
 
  for (int i = 0; i < N_CONFIGS; i++) {
    Serial.println();
    Serial.print("# --- config "); Serial.print(i + 1);
    Serial.print(" de "); Serial.print(N_CONFIGS);
    Serial.print(": "); Serial.println(cfgNota[i]);
 
    medirConfig(cfgBudget[i], cfgImp[i], cfgModo[i]);
 
    if (!rBusVivo) {
      Serial.println("#   EL BUS I2C NO RESPONDE. Barrido detenido.");
      Serial.println("#   Desconecta y reconecta el USB antes de reintentar.");
      break;
    }
 
    Serial.print(nombreModo(cfgModo[i])); Serial.print(",");
    Serial.print(cfgBudget[i]);           Serial.print(",");
    Serial.print(rBudgetReal);            Serial.print(",");
    Serial.print(cfgImp[i]);              Serial.print(",");
    Serial.print(rMedia / 1000.0, 3);     Serial.print(",");
    Serial.print(rSd, 1);                 Serial.print(",");
    Serial.print(rMin / 1000.0, 3);       Serial.print(",");
    Serial.print(rMax / 1000.0, 3);       Serial.print(",");
    Serial.print(rValidas);               Serial.print(",");
    Serial.print(rTotal);                 Serial.print(",");
    Serial.print(rRescates);              Serial.print(",");
    Serial.println(cfgNota[i]);
 
    if (rTotal > 0 && rValidas < (rTotal * 9) / 10)
      Serial.println("#   OJO: muchas muestras invalidas. El periodo de "
                     "este renglon puede estar mezclando regimenes.");
    if (rRescates > 0)
      Serial.println("#   (hubo rescates: se perdio algun flanco y el "
                     "codigo destrabo el pin por su cuenta)");
  }
 
  detachInterrupt(digitalPinToInterrupt(PIN_INT));
  Serial.println();
  Serial.println("# --- fin del barrido ---");
  Serial.println("# Lectura: si el periodo sigue al IMP cuando el IMP esta");
  Serial.println("# por encima de 37 ms, el IMP manda y el piso es lo unico");
  Serial.println("# que limita. Si el periodo se queda en ~37 ms pase lo que");
  Serial.println("# pase, es un piso del chip y va al presupuesto de error.");
}
 
void loop() { }
 