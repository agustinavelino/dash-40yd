// =====================================================================
// DASH Timing System - Corrida larga del VL53L1X apuntando al CIELO
//
// QUE PROBLEMA RESUELVE
//   El ADR-011 (IMP = 25 ms) solo se ha confirmado en regimen VALIDO
//   (blanco fijo). Las puertas de split y meta pasan casi todo el
//   tiempo en regimen INVALIDO (apuntando al cielo, sin retorno), y en
//   esa condicion nunca se ha medido a IMP 25. Faltan dos numeros
//   (Documento Maestro v2.2, 9.2 y 19):
//     1. la tasa de intervalos anomalos en regimen invalido a IMP 25
//        (a IMP 20 fue 13.7 %, cada uno ~9.3 ms mas largo);
//     2. que el margen de 0.94-2.8 ms no se agote: 0 saltos a 2 ranuras
//        (46.6 ms) en regimen valido.
//
// COMO FUNCIONA
//   Arranque en frio (reset por XSHUT + init una sola vez), modo corto,
//   budget 20 ms, IMP = IMP_MS. Captura N_MUESTRAS interrupciones
//   seguidas guardando, por muestra: timestamp de la ISR, range_status
//   crudo, distancia y si la muestra salio de la red de seguridad.
//   Nada se imprime durante la captura (regla 5 de CLAUDE.md): al final
//   sale una linea por muestra con checksum "*HH" y el marcador "# fin".
//   La separacion por regimen y el conteo de anomalos los hace
//   analizar_cielo.py sobre el crudo, no el firmware.
//
// COMO USARLO
//   1. Sensor apuntando al cielo (o a un fondo a mas de ~1.4 m, que el
//      modo corto ya no alcanza). Nada en el haz.
//   2. Arrancar el logger ANTES de energizar o reiniciar:
//        --fin "^# fin$" --max-s 300
//   3. Durante la captura (~2 min a IMP 25) el LED (GPIO2 integrado y
//      GPIO18 externo) se ENCIENDE 1 s cada 4 s: mientras esta encendido,
//      cruzar la mano o una cartulina por el haz a 0.3-1 m. Apagado, dejar
//      el haz libre. El estado del LED queda en la columna cue.
//   4. Tras ~117 s (ultimo ciclo), la placa imprime todo y termina.
//   Otro IMP (alternativa de la 9.2): compilar con -DIMP_MS=26, 27 o 28.
//
// CABLEADO (igual que el barrido v5)
//   VIN->3V3  GND->GND  SDA->GPIO21  SCL->GPIO22
//   GPIO1(INT)->GPIO16        XSHUT->GPIO17
//
// Libreria: "VL53L1X" de Pololu 1.3.1
// =====================================================================

#include <Wire.h>
#include <VL53L1X.h>

#define PIN_SDA     21
#define PIN_SCL     22
#define PIN_INT     16
#define PIN_XSHUT   17
#define PIN_LED     2       // LED integrado
#define PIN_CUE     18      // LED externo (anodo -> 330 ohm -> GPIO18, catodo -> GND)
#define CUE_PERIODO_MS 4000 // cada 4 s ...
#define CUE_ENCENDIDO_MS 1000 // ... el LED esta ENCENDIDO 1 s: CRUZA AHORA

#ifndef IMP_MS
#define IMP_MS      25      // ADR-011
#endif
#define BUDGET_US   20000   // el minimo del modo corto
#define N_DESCARTE  10
#define N_MUESTRAS  5000    // ~117 s a 23.3 ms; ~60 KB de RAM
#define ESPERA_S    5       // cuenta regresiva antes de capturar

VL53L1X sensor;

volatile int64_t  tINT = 0;
volatile bool     nueva = false;
volatile uint32_t nIsr = 0;          // flancos totales vistos por la ISR

// ISR: SOLO captura el timestamp y sale. Nada de I2C ni Serial aqui.
void IRAM_ATTR isrINT() {
  tINT = esp_timer_get_time();
  nueva = true;
  nIsr++;
}

// --- captura (se imprime al final) ---
int64_t  ts[N_MUESTRAS];
uint16_t dist[N_MUESTRAS];
uint8_t  st[N_MUESTRAS];             // range_status crudo de la libreria
uint8_t  cue[N_MUESTRAS];            // 1 = el LED pedia cruzar en ese instante
uint8_t  marca[N_MUESTRAS];          // 0 = ISR normal, 1 = rescate por nivel, 2 = la ISR vio > 1 flanco
int      nCap = 0, nRescates = 0, nDobles = 0;

// Reset por hardware. XSHUT no se fuerza a nivel alto: no es tolerante
// a 5 V y el modulo ya lo lleva en pull-up (regla 9).
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

// Linea con checksum XOR "*HH" (mismo formato que el Exp. 0b), para que
// el analisis descarte una linea corrupta en vez de aceptarla.
void lineaConChecksum(const char *s) {
  uint8_t x = 0;
  for (const char *p = s; *p; p++) x ^= (uint8_t)*p;
  Serial.printf("%s*%02X\n", s, x);
}

void setup() {
  Serial.begin(921600);
  delay(1000);
  pinMode(PIN_LED, OUTPUT); pinMode(PIN_CUE, OUTPUT);
  digitalWrite(PIN_LED, LOW); digitalWrite(PIN_CUE, LOW);

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);               // regla 10 (provisional)
  pinMode(PIN_INT, INPUT_PULLUP);

  resetSensor();
  sensor.setTimeout(500);
  if (!sensor.init()) {                // regla 8: una sola vez
    Serial.println("# FALLO init(). Revisa XSHUT (GPIO17) y SDA/SCL.");
    Serial.println("# fin");
    return;
  }
  sensor.setDistanceMode(VL53L1X::Short);
  sensor.setMeasurementTimingBudget(BUDGET_US);

  Serial.printf("# corrida_cielo | IMP %d ms | ranura teorica %.2f ms | budget pedido %d us, chip %lu us | %d muestras\n",
                IMP_MS, IMP_MS * 0.931, BUDGET_US,
                (unsigned long)sensor.getMeasurementTimingBudget(), N_MUESTRAS);
  for (int s = ESPERA_S; s > 0; s--) {
    Serial.printf("# empieza en %d s (apunta al cielo)\n", s);
    delay(1000);
  }
  Serial.println("# capturando: cruces intermitentes mientras parpadea el LED");
  Serial.flush();

  sensor.startContinuous(IMP_MS);
  delay(20);
  // Regla 7: la ISR se engancha DESPUES de startContinuous() y con INT
  // limpio; si se engancha estando abajo no hay flanco y todo se cuelga.
  limpiarIntPendiente();
  nueva = false;
  attachInterrupt(digitalPinToInterrupt(PIN_INT), isrINT, FALLING);

  const uint32_t limiteRescateUs = 2 * IMP_MS * 1000;   // > 2 periodos sin INT
  int desc = 0;
  uint32_t nIsrPrevio = 0;
  int64_t  tUltima = esp_timer_get_time();
  uint32_t tCue0 = millis();
  uint8_t  cueAhora = 0;

  while (nCap < N_MUESTRAS) {
    // LED encendido = cruza el haz. Es la referencia: el analisis compara las
    // muestras validas dentro y fuera de esa ventana.
    cueAhora = ((millis() - tCue0) % CUE_PERIODO_MS) < CUE_ENCENDIDO_MS;
    digitalWrite(PIN_LED, cueAhora); digitalWrite(PIN_CUE, cueAhora);

    uint8_t m;
    int64_t t;
    if (nueva) {
      noInterrupts();
      nueva = false;
      t = tINT;
      uint32_t n = nIsr;
      interrupts();
      m = (n - nIsrPrevio > 1) ? 2 : 0;          // se perdio una lectura entre dos flancos
      nIsrPrevio = n;
    } else if (esp_timer_get_time() - tUltima > limiteRescateUs && digitalRead(PIN_INT) == LOW) {
      // Red de seguridad (regla 7): INT abajo sin flanco visto. El
      // timestamp no es de la ISR; el analisis excluye esos intervalos.
      t = esp_timer_get_time();
      m = 1;
      nRescates++;
    } else {
      continue;
    }
    tUltima = t;

    uint16_t d = sensor.read(false);
    uint8_t  s = (uint8_t)sensor.ranging_data.range_status;
    if (desc < N_DESCARTE) { desc++; continue; }  // el arranque del continuo no cuenta
    if (m == 2) nDobles++;
    ts[nCap] = t; dist[nCap] = d; st[nCap] = s; marca[nCap] = m; cue[nCap] = cueAhora;
    nCap++;
  }

  detachInterrupt(digitalPinToInterrupt(PIN_INT));
  sensor.stopContinuous();
  digitalWrite(PIN_LED, LOW); digitalWrite(PIN_CUE, LOW);

  // --- impresion diferida (regla 5) ---
  delay(200);
  Serial.printf("# capturadas %d | rescates %d | dobles %d | duracion %.1f s\n",
                nCap, nRescates, nDobles, (ts[nCap - 1] - ts[0]) / 1e6);
  Serial.println("# M,k,t_us,status,dist_mm,marca,cue   (status 0 = RangeValid; cue 1 = LED encendido)");
  char l[64];
  for (int k = 0; k < nCap; k++) {
    snprintf(l, sizeof(l), "M,%d,%lld,%u,%u,%u,%u", k, (long long)ts[k], st[k], dist[k], marca[k], cue[k]);
    lineaConChecksum(l);
  }
  Serial.println("# fin");
}

void loop() { }
