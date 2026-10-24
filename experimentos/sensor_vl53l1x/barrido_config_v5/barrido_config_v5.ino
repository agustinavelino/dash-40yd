// =====================================================================
// DASH Timing System - Barrido del VL53L1X  (v5) - ARRANQUE EN FRIO
//
// QUE PROBLEMA RESUELVE
//   La v3 y la v4 se contradicen sobre la misma configuracion nominal:
//     v3 (estado heredado de la config anterior): IMP 25 -> 23.28 ms
//     v4 (reset por hardware + init cada config): todo a 2 ranuras
//   Las tres corridas en las que el chip arranco en frio (smoke test,
//   v4, y la config 1 de la v3) dieron 2 ranuras. El comportamiento de
//   1 ranura solo aparecio con estado heredado.
//
//   En produccion cada nodo arranca en frio, asi que esa es la
//   condicion que hay que medir. Este sketch la replica de forma
//   estricta: UNA configuracion por arranque, y entre una y otra se
//   reinicia el ESP32 completo.
//
// COMO FUNCIONA
//   Corre una sola configuracion, imprime el resultado, guarda el
//   indice en memoria RTC y llama a ESP.restart(). Al volver, toma la
//   siguiente. Asi cada medicion parte de:
//     - ESP32 recien arrancado (toda la libreria re-inicializada)
//     - sensor reseteado por hardware via XSHUT
//   que es exactamente el estado de un nodo al encenderse en campo.
//
//   Solo se flashea UNA vez. El monitor serial permanece abierto
//   durante los reinicios por software (la tarjeta usa un chip
//   USB-UART externo), asi que la salida se va acumulando sola.
//
// QUE SE BARRE
//   IMP de 10 a 25 ms. La zona baja es la importante: si el periodo es
//   un multiplo entero de la ranura, BAJAR el IMP BAJA el periodo, y
//   esa zona nunca se ha probado.
//
// COMO USARLO
//   1. Cartulina FIJA a ~1 m. No moverla en toda la corrida (son unos
//      4 minutos y 11 reinicios; el blanco debe ser el mismo en todos).
//   2. Abrir el monitor serial a 921600 ANTES de energizar.
//   3. Dejar correr hasta que imprima la tabla resumen.
//   4. Para repetirlo desde cero: desconectar y reconectar el USB
//      (un corte de energia borra la memoria RTC y reinicia el barrido).
//
// CABLEADO
//   VIN->3V3  GND->GND  SDA->GPIO21  SCL->GPIO22
//   GPIO1(INT)->GPIO16        XSHUT->GPIO17
//
// Libreria: "VL53L1X" de Pololu
// =====================================================================

#include <Wire.h>
#include <VL53L1X.h>

#define PIN_SDA     21
#define PIN_SCL     22
#define PIN_INT     16
#define PIN_XSHUT   17

#define BUDGET_US   20000   // fijo: el minimo del modo corto
#define N_DESCARTE  10
#define N_MEDIR     120

#define MAGIC       0xDA5AC001   // valor arbitrario; la v5 original traia una H (no es hex) y no compilaba

// IMP a barrer, de menor a mayor. La zona 10-16 es la que interesa.
const uint32_t cfgImp[] = { 10, 11, 12, 13, 14, 15, 16, 18, 20, 22, 25 };
const int N_CONFIGS = sizeof(cfgImp) / sizeof(cfgImp[0]);

// --- memoria RTC: sobrevive a ESP.restart(), no a un corte de energia ---
// RTC_NOINIT_ATTR y no RTC_DATA_ATTR: el bootloader reinicializa las
// RTC_DATA_ATTR en todo reinicio que no sea despertar de deep sleep, y la
// primera version del v5 se quedo en un bucle infinito en la config 1.
// NOINIT no se toca nunca; al encender trae basura, por eso existe MAGIC.
RTC_NOINIT_ATTR uint32_t rtcMagic;
RTC_NOINIT_ATTR int      rtcIdx;
RTC_NOINIT_ATTR float    rtcPer[16];     // periodo medio, ms
RTC_NOINIT_ATTR float    rtcSd[16];      // jitter de los limpios, us
RTC_NOINIT_ATTR float    rtcRanuras[16]; // periodo / ranura teorica
RTC_NOINIT_ATTR int16_t  rtcAnom[16];    // intervalos anomalos
RTC_NOINIT_ATTR int16_t  rtcNum[16];     // intervalos contados
RTC_NOINIT_ATTR int16_t  rtcDist[16];    // distancia media, mm
RTC_NOINIT_ATTR int16_t  rtcVal[16];     // muestras validas

VL53L1X sensor;

volatile int64_t tINT = 0;
volatile bool    nueva = false;

void IRAM_ATTR isrINT() {
  tINT = esp_timer_get_time();
  nueva = true;
}

// --- captura ---
int64_t  ts[N_MEDIR];
bool     okM[N_MEDIR];
uint16_t dM[N_MEDIR];
int      nCap = 0;
int      nRescates = 0;

// --- estadistica ---
double  eMedia = 0, eSd = 0, eMediana = 0;
int     eN = 0, eAnom = 0;
int64_t eMin = 0, eMax = 0;

// Reset por hardware. XSHUT no se fuerza a nivel alto: no es tolerante
// a 5 V y el modulo ya lo lleva en pull-up, asi que se suelta en alta
// impedancia. Seccion 12.2 del Documento Maestro.
void resetSensor() {
  pinMode(PIN_XSHUT, OUTPUT);
  digitalWrite(PIN_XSHUT, LOW);
  delay(10);
  pinMode(PIN_XSHUT, INPUT);
  delay(10);
}

void limpiarIntPendiente() {
  for (int i = 0; i < 6 && digitalRead(PIN_INT) == LOW; i++) {
    sensor.read(false);
    delay(5);
  }
}

bool esperarMuestra(uint32_t limite_ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < limite_ms) {
    if (nueva) { nueva = false; return true; }
    if (digitalRead(PIN_INT) == LOW) {
      sensor.read(false);
      nRescates++;
      delay(2);
    }
  }
  return false;
}

// Mediana por insercion, luego media y desviacion de los intervalos
// "limpios" (<= 1.3x la mediana). Un intervalo por encima de ese corte
// no es ruido: es el chip saltando a otra ranura entera.
void estadistica(int64_t *d, int n) {
  eN = n; eMedia = eSd = eMediana = 0; eAnom = 0; eMin = eMax = 0;
  if (n < 3) return;

  static int64_t s[N_MEDIR];
  for (int i = 0; i < n; i++) s[i] = d[i];
  for (int i = 1; i < n; i++) {
    int64_t v = s[i]; int j = i - 1;
    while (j >= 0 && s[j] > v) { s[j+1] = s[j]; j--; }
    s[j+1] = v;
  }
  eMediana = (n % 2) ? (double)s[n/2]
                     : (double)(s[n/2 - 1] + s[n/2]) / 2.0;

  double corte = eMediana * 1.3, suma = 0, suma2 = 0;
  int nLimpios = 0;
  for (int i = 0; i < n; i++) {
    if (eMin == 0 || d[i] < eMin) eMin = d[i];
    if (d[i] > eMax) eMax = d[i];
    if ((double)d[i] > corte) { eAnom++; continue; }
    suma  += (double)d[i];
    suma2 += (double)d[i] * (double)d[i];
    nLimpios++;
  }
  if (nLimpios < 2) return;
  eMedia = suma / nLimpios;
  double var = suma2 / nLimpios - eMedia * eMedia;
  eSd = var > 0 ? sqrt(var) : 0;
}

void imprimirResumen() {
  Serial.println();
  Serial.println("# ===== RESUMEN DEL BARRIDO EN FRIO =====");
  Serial.println("imp_ms,ranura_ms,periodo_ms,ranuras,jitter_us,"
                 "anomalos,n,pct_anom,dist_mm,validas");
  for (int i = 0; i < N_CONFIGS; i++) {
    if (rtcNum[i] <= 0) {
      Serial.print(cfgImp[i]); Serial.println(",,,,,,,,,SIN DATOS");
      continue;
    }
    Serial.print(cfgImp[i]);                      Serial.print(",");
    Serial.print(cfgImp[i] * 0.931, 2);           Serial.print(",");
    Serial.print(rtcPer[i], 3);                   Serial.print(",");
    Serial.print(rtcRanuras[i], 3);               Serial.print(",");
    Serial.print(rtcSd[i], 1);                    Serial.print(",");
    Serial.print(rtcAnom[i]);                     Serial.print(",");
    Serial.print(rtcNum[i]);                      Serial.print(",");
    Serial.print(100.0 * rtcAnom[i] / rtcNum[i], 1); Serial.print(",");
    Serial.print(rtcDist[i]);                     Serial.print(",");
    Serial.print(rtcVal[i]); Serial.print("/");   Serial.println(N_MEDIR);
  }
  Serial.println();
  Serial.println("# Que mirar:");
  Serial.println("#  - columna 'ranuras': debe dar un entero (1, 2, 3...).");
  Serial.println("#    Confirma que el periodo es multiplo de IMP x 0.931.");
  Serial.println("#  - el IMP con menor periodo_ms y 0 % de anomalos y");
  Serial.println("#    validas = 120/120 es el candidato a reemplazar");
  Serial.println("#    el ADR-011.");
  Serial.println("#  - si 'dist_mm' varia mucho entre renglones, el blanco");
  Serial.println("#    se movio y la comparacion no es limpia.");
  Serial.println();
  Serial.println("# Barrido terminado. Desconecta y reconecta el USB");
  Serial.println("# para repetirlo desde cero.");
}

void setup() {
  Serial.begin(921600);
  delay(2000);                 // da tiempo a que el monitor reenganche

  // Primer arranque tras un corte de energia: inicializa la memoria RTC.
  if (rtcMagic != MAGIC) {
    rtcMagic = MAGIC;
    rtcIdx = 0;
    for (int i = 0; i < 16; i++) {
      rtcPer[i] = rtcSd[i] = rtcRanuras[i] = 0;
      rtcAnom[i] = rtcNum[i] = rtcDist[i] = rtcVal[i] = 0;
    }
    Serial.println();
    Serial.println("# ===================================================");
    Serial.println("# Barrido VL53L1X en ARRANQUE EN FRIO - DASH (v5)");
    Serial.println("# Una configuracion por arranque del ESP32.");
    Serial.println("# Cartulina FIJA a ~1 m. No moverla en toda la corrida.");
    Serial.print(  "# Configuraciones: "); Serial.println(N_CONFIGS);
    Serial.println("# ===================================================");
  }

  if (rtcIdx >= N_CONFIGS) { imprimirResumen(); while (1) delay(1000); }

  uint32_t imp = cfgImp[rtcIdx];
  double ranura = imp * 0.931;

  Serial.println();
  Serial.print("# --- config "); Serial.print(rtcIdx + 1);
  Serial.print(" de "); Serial.print(N_CONFIGS);
  Serial.print(" | IMP = "); Serial.print(imp);
  Serial.print(" ms | ranura teorica "); Serial.print(ranura, 2);
  Serial.println(" ms ---");

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  pinMode(PIN_INT, INPUT_PULLUP);

  resetSensor();
  sensor.setTimeout(500);
  if (!sensor.init()) {
    Serial.println("#   FALLO init(). Revisa XSHUT (GPIO17) y SDA/SCL.");
    rtcIdx++;
    delay(200);
    ESP.restart();
  }

  sensor.setDistanceMode(VL53L1X::Short);
  sensor.setMeasurementTimingBudget(BUDGET_US);
  Serial.print("#   budget: pedido "); Serial.print(BUDGET_US);
  Serial.print(" us, chip reporta ");
  Serial.print(sensor.getMeasurementTimingBudget()); Serial.println(" us");

  sensor.startContinuous(imp);
  delay(20);

  // La ISR se engancha DESPUES de arrancar el continuo y con el pin ya
  // limpio: INT queda latcheado en bajo y si se engancha estando abajo
  // no hay flanco de bajada que detectar.
  limpiarIntPendiente();
  nueva = false;
  attachInterrupt(digitalPinToInterrupt(PIN_INT), isrINT, FALLING);

  int desc = 0;
  while (desc < N_DESCARTE) {
    if (!esperarMuestra(1500)) break;
    sensor.read();
    desc++;
  }

  if (desc < N_DESCARTE) {
    Serial.println("#   no produce INT de forma sostenida.");
  } else {
    nCap = 0;
    while (nCap < N_MEDIR) {
      if (!esperarMuestra(1500)) break;
      ts[nCap] = tINT;
      dM[nCap] = sensor.read();
      okM[nCap] =
        (sensor.ranging_data.range_status == VL53L1X::RangeValid);
      nCap++;
    }

    // Solo intervalos entre dos muestras validas: es el regimen que
    // determina el presupuesto de error. Las invalidas y las de
    // transicion se descartan.
    static int64_t dVal[N_MEDIR];
    int nV = 0, nValidas = 0;
    long sumaDist = 0;
    for (int k = 0; k < nCap; k++)
      if (okM[k]) { nValidas++; sumaDist += dM[k]; }
    for (int k = 1; k < nCap; k++) {
      int64_t d = ts[k] - ts[k-1];
      if (d > 0 && okM[k] && okM[k-1]) dVal[nV++] = d;
    }

    if (nV < 3) {
      Serial.print("#   solo "); Serial.print(nV);
      Serial.println(" intervalos validos: el blanco no se esta viendo bien.");
    } else {
      estadistica(dVal, nV);
      rtcPer[rtcIdx]     = eMedia / 1000.0;
      rtcSd[rtcIdx]      = eSd;
      rtcRanuras[rtcIdx] = (eMedia / 1000.0) / ranura;
      rtcAnom[rtcIdx]    = eAnom;
      rtcNum[rtcIdx]     = eN;
      rtcDist[rtcIdx]    = nValidas ? (sumaDist / nValidas) : 0;
      rtcVal[rtcIdx]     = nValidas;

      Serial.print("#   periodo "); Serial.print(eMedia / 1000.0, 3);
      Serial.print(" ms = "); Serial.print(rtcRanuras[rtcIdx], 3);
      Serial.print(" ranuras | jitter "); Serial.print(eSd, 1);
      Serial.print(" us | anomalos "); Serial.print(eAnom);
      Serial.print("/"); Serial.print(eN);
      Serial.print(" | min "); Serial.print(eMin / 1000.0, 2);
      Serial.print(" max "); Serial.print(eMax / 1000.0, 2);
      Serial.print(" | dist "); Serial.print(rtcDist[rtcIdx]);
      Serial.print(" mm | validas "); Serial.print(nValidas);
      Serial.print("/"); Serial.print(nCap);
      if (nRescates) { Serial.print(" | rescates "); Serial.print(nRescates); }
      Serial.println();
    }
  }

  detachInterrupt(digitalPinToInterrupt(PIN_INT));
  rtcIdx++;

  if (rtcIdx >= N_CONFIGS) { imprimirResumen(); while (1) delay(1000); }

  Serial.flush();
  delay(300);
  ESP.restart();              // arranque en frio para la siguiente
}

void loop() { }
