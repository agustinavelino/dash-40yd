# Caracterización del VL53L1X — cadencia de INT y elección del IMP

**Resultado:** a IMP = 25 ms el periodo real es **23.31 ms (1 ranura) en arranque en frío** → cuantización σ = **6.73 ms** · presupuesto de la sección 6: ±10 ms · **margen de 1.5×** · Estado: **APROBADO** en régimen válido y sin retorno (fondo de vidrio, sección 11; cielo abierto, sección 12). **Hallazgo:** al entrar un blanco el INT cambia de fase ~9.9 ms en todos los cruces (sección 12.5)

---

## 1. Objetivo

Medir la cadencia real con la que el VL53L1X avisa por INT y elegir el inter-measurement period (IMP) que la hace más corta. La cadencia fija la ranura de medición, y la ranura fija el error de cuantización, que es la contribución del sensor al presupuesto de error (sección 6).

La afirmación bajo prueba: *con IMP = 25 ms el chip entrega una medición cada ranura (23.28 ms) también en arranque en frío, que es como arranca cada nodo en campo.*

Este reporte junta tres corridas: el smoke test y el barrido v3 (11-oct-2026) y el barrido v5 en arranque en frío (23-oct-2026).

## 2. Principio metrológico

El periodo entre interrupciones se mide con `esp_timer_get_time()` capturado como primera instrucción de la ISR, la misma técnica del Experimento 0, donde el jitter puro de captura resultó de **1.3 µs**. Eso está cuatro órdenes de magnitud por debajo de la ranura (~23 ms) y dos por debajo del jitter de cadencia observado (≥ 48 µs). Todo lo que se mide como variación del periodo es del chip, no de la captura.

Valor esperado: si el modelo de la sección 9.2 es correcto, el periodo es un **múltiplo entero** de la ranura, con ranura = IMP × 0.931.

## 3. Montaje

| Señal | Módulo TOF400C-VL53L1X | ESP32-WROOM-32U (hub) |
|---|---|---|
| Alimentación | VIN | 3V3 |
| Tierra | GND | GND |
| I2C datos | SDA | GPIO21 |
| I2C reloj | SCL | GPIO22 |
| Interrupción | GPIO1 (INT) | GPIO16 |
| Reset por hardware | XSHUT | GPIO17 (solo v5) |

- Protoboard con jumpers; I2C a **100 kHz** (400 kHz se caía, provisional, sección 12.2).
- Modo corto, timing budget pedido 20 000 µs (el chip reporta **20 024 µs**).
- Blanco: cartulina **fija** a 1104–1105 mm (v5). Las corridas del 11-oct se hicieron sobre el mismo montaje sin XSHUT.
- Placa identificada por MAC (`a4:f0:0f:64:ac:68`, hub) en COM7.

## 4. Modelo de cadencia

El chip divide el tiempo en ranuras de ancho fijo:

$$\text{ranura} = \text{IMP} \times 0.931$$

El 0.931 es un error de escala del oscilador interno, medido en el smoke test con tres IMP independientes (25, 33 y 40 ms → 0.9313, 0.9309, 0.9307).

Una medición dura un tiempo `T`. Si no cabe en una ranura, el chip espera a la siguiente frontera, así que el periodo es

$$\text{periodo} = n \times \text{ranura}, \qquad n = \left\lceil \frac{T}{\text{ranura}} \right\rceil$$

Y la cuantización por sensor, suponiendo ventanas uniformes:

$$\sigma_q = \frac{\text{periodo}}{\sqrt{12}}$$

## 5. Procedimiento

**Smoke test (11-oct-2026).** Escaneo I2C, `init()` (valida el Model ID 0xEACC: es VL53L1X, no VL53L0X), modo corto, budget 20 ms, IMP 20 ms, captura continua con ISR. 648 muestras separadas después por régimen (con blanco y sin retorno).

**Barrido v3 (11-oct-2026).** Varias configuraciones de IMP en un solo arranque, con `stopContinuous → configurar → startContinuous` entre una y otra. 8 muestras de descarte y 50 de medición por configuración.

**Barrido v5 (23-oct-2026).**

1. Una sola configuración por arranque del ESP32. Entre una y otra, `ESP.restart()` y reset del sensor por XSHUT: cada medición parte del estado de un nodo recién encendido.
2. IMP barrido: 10, 11, 12, 13, 14, 15, 16, 18, 20, 22 y 25 ms.
3. Por configuración, 10 muestras de descarte y 120 de medición. Solo entran a la estadística los intervalos entre dos muestras válidas.
4. Los resultados se guardan en memoria RTC (`RTC_NOINIT_ATTR`) y al final se imprime la tabla resumen.
5. Captura con `tools/logger_serial.py` a 921 600 baud, desde el arranque hasta el marcador `Barrido terminado`: **87 líneas en 70 s**.

**Control de la corrida v5:** 11/11 configuraciones, 1320/1320 muestras válidas, 0/1309 intervalos anómalos.

## 6. Resultados

![Periodo real contra IMP](01_periodo_vs_imp.png)

Las 11 configuraciones caen sobre las rectas de 1, 2 o 3 ranuras. La desviación máxima del cociente periodo/ranura respecto al entero es **0.009**. El modelo de la sección 4 se confirma.

### 6.1 Predicción contra resultado

| | Predicción (escrita el 23-oct-2026, en `docs/ESTADO.md`) | Medido |
|---|---|---|
| Ranuras a IMP 25 en frío | Sin predicción numérica del usuario. La evidencia previa apuntaba a **2 ranuras** (smoke test, v4 y config 1 del v3, todas en frío) | **1.001 ranuras (23.31 ms)** |

Esto es un **hallazgo**. La hipótesis "en arranque en frío todo da 2 ranuras" queda refutada. Las corridas en frío que dieron 2 ranuras se explican sin ella: el smoke test usaba IMP 20 (ranura 18.62 ms < T), que da 2 ranuras por el modelo. El v4 no está en el repo y no se puede revisar (ver sección 9).

### 6.2 Barrido v5 completo

| IMP (ms) | Ranura (ms) | Periodo (ms) | Ranuras | Jitter (µs) | Anómalos | Válidas |
|---|---|---|---|---|---|---|
| 10 | 9.31 | 28.011 | 3.009 | 265.8 | 0/119 | 120/120 |
| 11 | 10.24 | 30.804 | 3.008 | 175.9 | 0/119 | 120/120 |
| 12 | 11.17 | **22.405** | 2.006 | 64.7 | 0/119 | 120/120 |
| 13 | 12.10 | 24.265 | 2.005 | 105.0 | 0/119 | 120/120 |
| 14 | 13.03 | 26.128 | 2.005 | 185.8 | 0/119 | 120/120 |
| 15 | 13.97 | 27.991 | 2.004 | 241.7 | 0/119 | 120/120 |
| 16 | 14.90 | 29.849 | 2.004 | 266.1 | 0/119 | 120/120 |
| 18 | 16.76 | 33.568 | 2.003 | 71.3 | 0/119 | 120/120 |
| 20 | 18.62 | 37.288 | 2.003 | 56.9 | 0/119 | 120/120 |
| 22 | 20.48 | 41.011 | 2.002 | 48.4 | 0/119 | 120/120 |
| 25 | 23.28 | **23.308** | 1.001 | 235.2 | 0/119 | 120/120 |

### 6.3 Duración real de una medición

El barrido acota `T` sin medirlo directamente:

- IMP 22 da 2 ranuras → una medición **no cabe** en 20.48 ms.
- IMP 12 da 2 ranuras de 11.17 ms → una medición **sí cabe** en 22.34 ms.

$$20.48 \text{ ms} < T \le 22.34 \text{ ms}$$

`T` es mayor que el budget reportado (20.02 ms) en al menos 0.46 ms: el chip gasta tiempo fuera de la integración. Consecuencia: el margen a IMP 25 es **0.94–2.80 ms**, no los 3.3 ms de la sección 9.2, que suponían `T` = budget. Firmeza: **firme** como cota (11 configuraciones consistentes); el valor exacto de `T` no está medido.

### 6.4 Repetibilidad entre arranques

![Repetibilidad a IMP 10](02_repetibilidad_imp10.png)

El primer intento del v5 se quedó en un bucle en la config 1 (sección 9), y eso dio sin querer **92 arranques en frío seguidos a IMP 10**: periodo medio entre **27.996 y 28.016 ms**, con desviación estándar entre arranques de **4.0 µs**. El chip llega a la misma cadencia en cada arranque. Se ve una subida lenta de ~0.015 ms en los primeros ~20 arranques. Es compatible con calentamiento, pero no está verificado.

### 6.5 Jitter de cadencia

| Fuente | Condición | Jitter (1σ) |
|---|---|---|
| Documento Maestro 9.2 (11-oct) | IMP 25, 51 muestras | 10.6 µs |
| Smoke test (11-oct) | IMP 20, régimen válido, 404 intervalos | 81 µs |
| Smoke test (11-oct) | IMP 20, régimen inválido, 241 intervalos | 266 µs |
| v5 (23-oct) | 11 configuraciones, régimen válido | 48–266 µs (235 µs a IMP 25) |
| v5 intento 1 (23-oct) | IMP 10, misma configuración, 92 arranques | 44–315 µs |

El jitter **varía de una corrida a otra en la misma configuración** (44 a 315 µs a IMP 10). El valor de 10.6 µs no se reproduce y debe dejar de considerarse firme. Aun en el peor caso (315 µs), es el **1.4 %** de la ranura y queda 20× por debajo de la cuantización (6.73 ms). No cambia ninguna decisión.

### 6.6 Hallazgos del smoke test (11-oct-2026, sección 9.2–9.4 del Documento Maestro)

No hay datos crudos de estas corridas en el repo; las cifras vienen del Documento Maestro v2.1.

| Hallazgo | Valor | Firmeza (DM 0.2) |
|---|---|---|
| Factor de escala de la ranura | 0.931 (3 IMP independientes) | Firme |
| Dos regímenes a IMP 20 | 18.6 ms sin blanco, 37.24 ms con blanco | Firme |
| Intervalos anómalos, régimen inválido, IMP 20 | 33/241 (13.7 %), cada uno ~9.3 ms más largo | Firme a IMP 20; sin medir a IMP 25 |
| Intervalos anómalos, régimen válido, IMP 20 | 0/404 | Firme |
| Alcance útil en modo corto | 1.38 m (corte entre 1372 y 1395 mm, 4 transiciones) | Firme |
| Pin INT | Activo en bajo y latcheado hasta `read()` | Verificado en banco |
| Ancho del pulso bajo de INT | ~3.5 ms a 100 kHz (15 % de la ranura) | Medido |

## 7. Contexto en el presupuesto de error

| Fuente | Estimado (v2.0) | v2.1 (11-oct) | Este reporte |
|---|---|---|---|
| Geometría corporal | ±20–50 ms | — | — (dominante, irreducible) |
| **Cuantización del sensor** | ±10 ms | ±6.72 ms (piso), ~7.5 ms esperado | **±6.73 ms confirmado en frío**, régimen válido |
| Sincronización entre nodos | ±1–2 ms | ±0.17 ms (Exp. 0) | — |
| Jitter de cadencia del chip | — | 10.6 µs | 48–315 µs (despreciable) |

La cuantización sigue dentro del presupuesto de ±10 ms, con margen de 1.5×. El valor esperado de ~7.5 ms depende de la tasa de intervalos anómalos en régimen inválido a IMP 25, que no se ha medido.

## 8. Conclusiones

1. **El ADR-011 (IMP = 25 ms) se sostiene en arranque en frío.** Periodo de 23.31 ms (1.001 ranuras), 120/120 válidas, 0 anómalos. Antes solo se tenía con el estado heredado del v3.
2. **El modelo periodo = n × ranura queda confirmado en 11 configuraciones** (desviación máxima 0.009 ranuras). La cadencia es predecible a partir del IMP, sin sorpresas.
3. **El margen a IMP 25 es de 0.94–2.80 ms, no de 3.3 ms.** Una medición dura entre 20.48 y 22.34 ms, más que el budget de 20.02 ms. Eso hace más importante la corrida larga en régimen inválido (sección 19): si el margen se agota, el periodo salta a 46.6 ms.
4. **IMP 12 es la única alternativa más rápida**: 22.41 ms (−0.9 ms, cuantización 6.47 ms, −4 %), con un margen de 0–1.86 ms. La ganancia no mueve el presupuesto. Se recomienda **mantener IMP 25** (preferencia respaldada por el margen; decisión abierta en `docs/ESTADO.md`).
5. **El jitter de cadencia de 10.6 µs no es reproducible**: varía entre 44 y 315 µs según la corrida. Es irrelevante frente a la cuantización, pero su firmeza en el Documento Maestro debe bajar.
6. **Régimen inválido a IMP 25: medido** (secciones 11 y 12). Al cielo abierto: 3.0 % de anómalos sin blanco y 0 saltos a 2 ranuras con blanco. La cadencia se cierra; queda abierto el efecto del cambio de fase en el instante del cruce (12.5), que mide el Experimento 1.

## 9. Incidencias resueltas durante la ejecución

| Síntoma | Causa raíz | Solución |
|---|---|---|
| "Sin interrupciones" en el barrido v2 | `attachInterrupt(FALLING)` antes de `startContinuous()` con INT ya latcheado en bajo: nunca hubo flanco | v3: ISR después de `startContinuous()`, limpiando el pin antes; rescate por nivel si pasan > 2 periodos |
| "Bus no responde" en v2 | `init()` en cada configuración: el reset por software deja el chip en mal estado | `init()` una sola vez; reconfigurar con `stopContinuous → … → startContinuous` |
| Bus I2C caído | 400 kHz en protoboard con jumpers largos | 100 kHz (provisional, re-medir en PCB) |
| v3 y v4 se contradicen en la misma configuración (1 contra 2 ranuras) | Hipótesis: estado heredado entre configuraciones | v5: una configuración por arranque. Resultado: en frío, IMP 25 da 1 ranura. **La contradicción con el v4 queda sin explicar**: su sketch y sus datos no están en el repo |
| El v5 no compilaba | `#define MAGIC 0xDA5HC001`: la H no es hexadecimal | `0xDA5AC001` (valor arbitrario) |
| El v5 repetía la config 1 indefinidamente (97 arranques en 10 min) | `RTC_DATA_ATTR` se reinicializa en todo reinicio que no sea despertar de deep sleep | `RTC_NOINIT_ATTR`; el `MAGIC` ya cubría la basura del encendido |

Hipótesis descartada con datos: **"en arranque en frío todo da 2 ranuras"**. IMP 25 en frío dio 1.001 ranuras.

## 10. Archivos

| Archivo | Contenido |
|---|---|
| `smoke_test_vl53l1x/smoke_test_vl53l1x.ino` | Smoke test: escaneo I2C, Model ID, captura continua con ISR |
| `barrido_config_v3/barrido_config_v3.ino` | Barrido con varias configuraciones por arranque (estado heredado) |
| `barrido_config_v5/barrido_config_v5.ino` | Barrido en arranque en frío, una configuración por arranque (vigente) |
| `barrido_v5_frio_2026-10-23.txt` | Salida serial cruda del v5: 11 configuraciones y tabla resumen |
| `barrido_v5_intento1_bucle_2026-10-23.txt` | Intento 1 del v5 (bucle en la config 1): 92 mediciones completas a IMP 10 |
| `graficar_sensor.py` | Genera las gráficas 01–02 e imprime los números derivados de este reporte |
| `01_periodo_vs_imp.png` | Periodo real contra IMP, con las rectas n × ranura y la banda de T |
| `02_repetibilidad_imp10.png` | Periodo medio en 92 arranques en frío a IMP 10 |
| `corrida_cielo/corrida_cielo.ino` | Corrida larga: 5000 interrupciones en arranque en frío, LED de cruce en GPIO18 (sección 11) |
| `analizar_cielo.py` | Separa intervalos por régimen, cuenta anómalos y saltos, verifica el fondo con la columna `cue` |
| `corrida_cielo_IMP25_2026-10-24.txt` | Intento 1: no válido (sin LED visible, cruces irregulares) |
| `corrida_cielo_IMP25_2026-10-24_2.txt` | Intento 2: cortado a mano (LED sin GND), sin muestras |
| `corrida_cielo_IMP25_2026-10-24_3.txt` | Intento 3: la corrida válida con fondo de vidrio |
| `corrida_cielo_IMP25_2026-10-26.txt` | S2b: corrida válida al cielo abierto (sección 12) |
| `corrida_cielo_IMP25_2026-10-26_intento1_idle-corto.txt` | S2b, intento 1: cortado por el logger (`--idle-s 30`), solo ruido de arranque |
| `fase_cruces.py` | Fase de cada interrupción: tardías, tempranas y fase de la primera muestra válida de cada cruce (sección 12.5) |

No están en el repo: los datos crudos del smoke test y del v3, y el sketch y los datos del v4.

## 11. Corrida larga en régimen sin retorno (24-oct-2026, fondo de vidrio)

**Resultado:** a IMP 25, **0 saltos a 46.6 ms en 1 054 intervalos con blanco** y **3.3 % de intervalos anómalos sin blanco**: anómalos de fase (±9.5 ms), no ranuras perdidas. El ADR-011 se sostiene. Falta repetirla apuntando al **cielo abierto** (S2b): esta se hizo contra un vidrio a ~2 m.

### 11.1 Objetivo

Medir la condición en la que las puertas de split y meta pasan casi todo el tiempo: sin nada en el haz (régimen inválido). Dos preguntas abiertas (Maestro v2.2, 9.2 y 19):
1. ¿Se agota el margen de 0.94–2.8 ms y el periodo salta a 2 ranuras (46.6 ms)?
2. ¿Qué tasa de intervalos anómalos hay sin blanco a IMP 25? A IMP 20 fue 13.7 %, todos +9.3 ms.

### 11.2 Montaje y procedimiento

- Sensor en la placa hub (COM7), cableado del v5 (XSHUT a GPIO17), modo corto, budget 20 ms, IMP 25, arranque en frío.
- **Fondo:** vidrio a ~2 m, más allá del alcance del modo corto (1.38 m). El sensor lo reporta con status 4 (fuera de rango).
- **Cruces guiados:** un LED externo en GPIO18 se enciende 1 s cada 4 s. Con el LED encendido el usuario cruza la mano o una cartulina a 0.3–1 m; apagado, deja el haz libre. El estado del LED se guarda en cada muestra (columna `cue`).
- 5 000 interrupciones capturadas (~116 s); impresión diferida al final con checksum.
- Análisis: `analizar_cielo.py`. Separa cada intervalo por el status de sus dos extremos (válido, inválido o transición) y lo clasifica en 1 ranura, 2 ranuras o anómalo (tolerancia ±0.15 ranuras).

### 11.3 Predicción contra resultado

| | Predicción (escrita el 24-oct-2026) | Medido |
|---|---|---|
| Corrida larga | **Sin predicción**: el usuario no la hizo, registrado antes de correr | — |
| Saltos a 2 ranuras con blanco | — | **0 / 1 054** |
| Anómalos sin blanco | Referencia: 13.7 % a IMP 20 | **3.3 % (127 / 3 878)** |

### 11.4 Resultados (intento 3)

| Régimen | Intervalos | 1 ranura | 2 ranuras | Anómalos | Mediana | Jitter de los normales |
|---|---|---|---|---|---|---|
| Válido (blanco en el haz) | 1 054 | 1 054 | 0 | 0 (0.0 %) | 23.293 ms | 133 µs |
| Inválido (sin blanco) | 3 878 | 3 751 | 0 | 127 (3.3 %) | 23.282 ms | 38 µs |
| Transición | 62 | 33 | 0 | 29 (46.8 %) | 23.305 ms | 70 µs |

- **El fondo no da retorno.** Con el LED apagado desde hace más de 1 s, solo 1 de 2 473 muestras salió válida (0.04 %). Las válidas del primer segundo tras apagarse el LED son la mano saliendo, a ~40 cm: 665 de 666 caen en ese segundo.
- **31 cruces** en 116 s. Con el LED encendido solo el 34 % de las muestras fue válido: es la reacción humana (la mano llega tarde). No afecta la medición; solo dice que los cruces duran menos que la ventana del LED.
- **Los anómalos son de fase, no ranuras perdidas.** Hay exactamente 78 intervalos cortos (−9.5 ms) y 78 largos (+9.5 ms), y casi nunca vienen seguidos (2 pares). Sin blanco, el INT se adelanta ~9.5 ms y se queda adelantado; al entrar un blanco regresa a su fase (en las transiciones, la mediana del exceso es +9.5 ms). El periodo medio de toda la corrida es 23.284 ms: no se pierde ninguna ranura. *(Corregido el 26-oct-2026 en la sección 12.5: los anómalos son casi todos una sola interrupción tardía, un par + − seguido. La fase adelantada sin blanco sí se confirma.)*
- Hipótesis, **sin verificar**: sin retorno la medición aborta antes de completar el budget (la 9.2 ya lo describe a IMP 20) y el INT sale antes.

### 11.5 Lo que esto cambia

1. **ADR-011 (IMP 25) se sostiene:** el margen no se agotó en 1 054 intervalos con blanco.
2. **El modelo de "ventanas de ancho variable" de la 9.2 hay que revisarlo.** Se escribió para anómalos +9.3 ms al 13.7 % (σ de cuantización 7.54 ms; predicción del Exp. 1 de 10.7 ms). A IMP 25 los anómalos son 3.3 % y simétricos.
3. **El instante del cruce es justo donde aparecen** (47 % de las transiciones lleva el +9.5 ms). El Experimento 1 medirá si eso agrega error a la detección.
4. Los cambios al Maestro esperan a S2b (decisión del usuario): están en `docs/ESTADO.md`, "Cambios pendientes para el Documento Maestro v2.3".

### 11.6 Incidencias

| Síntoma | Causa raíz | Solución |
|---|---|---|
| Intento 1: el usuario no vio el LED y cruzó pocas veces, sin patrón | El LED integrado (GPIO2) no se ve bien y no indicaba cuándo cruzar | LED externo en GPIO18 con ciclo de 1 s encendido cada 4 s, y columna `cue`. El intento 1 se conserva y se marca como no válido: con 12.7 % de válidas no se distinguen cruces de lecturas de la cortina |
| Intento 2: el LED externo no encendía | Faltaba el GND del LED | Cortado a mano; se conserva el archivo parcial, sin muestras |
| Intento 3: el logger cortó a los 300 s imprimiendo las últimas 5 muestras | La impresión tardó mucho más que en el intento 1 (125 s en total); causa sin aislar | 4 995 / 5 000 muestras; no afecta. Para S2b, logger con `--max-s 400` |
| El análisis contaba como "fondo con retorno" la mano saliendo tarde | Reacción humana al apagarse el LED | `analizar_cielo.py` verifica el fondo solo con muestras de más de 1 s después de apagarse el LED |

## 12. Corrida larga al cielo abierto (S2b, 26-oct-2026)

**Resultado:** a IMP 25, **0 saltos a 46.6 ms en 1 022 intervalos con blanco** y **3.0 % de anómalos sin blanco** (vidrio: 3.3 %). Fondo verificado: 0 de 2 471 muestras válidas con el LED apagado hace más de 1 s. **El ADR-011 se sostiene al cielo abierto. S2 queda cerrada.**

### 12.1 Montaje y procedimiento

Igual que la sección 11 (mismo sketch `corrida_cielo.ino`, IMP 25, LED de cruce en GPIO18, placa hub en COM7), con una diferencia: **el sensor apunta al cielo abierto**, no a un vidrio. Arranque en frío al abrir el puerto. Logger con `--fin "^# fin$" --max-s 400`. 5 000 muestras en 116.4 s, todas con checksum válido, 0 rescates, 0 lecturas perdidas. 30 cruces.

### 12.2 Predicción contra resultado

| | Predicción (escrita el 26-oct-2026) | Medido |
|---|---|---|
| Corrida al cielo | **Sin predicción**: el usuario no la hizo, registrado antes de correr | — |
| ≥ 1 000 intervalos sin blanco | criterio | **3 918**: cumple |
| Saltos a 2 ranuras con blanco | criterio: 0 | **0 / 1 022**: cumple |
| Válidas con LED apagado > 1 s | criterio: < 1 % | **0 / 2 471 (0.00 %)**: cumple |
| Anómalos sin blanco | referencia: vidrio 3.3 %, IMP 20 13.7 % | **3.0 % (118 / 3 918)** |

### 12.3 Resultados

| Régimen | Intervalos | 1 ranura | 2 ranuras | Anómalos | Mediana | Jitter de los normales |
|---|---|---|---|---|---|---|
| Válido (blanco en el haz) | 1 022 | 1 022 | 0 | 0 (0.0 %) | 23.299 ms | 36 µs |
| Inválido (cielo) | 3 918 | 3 800 | 0 | 118 (3.0 %) | 23.289 ms | 73 µs |
| Transición | 59 | 14 | 0 | 45 (76.3 %) | 23.309 ms | 28 µs |

- Status de las 5 000 muestras: 0 (válida) 1 052 · 1 87 · **2 (signal fail) 2 888** · 4 (fuera de rango) 967 · 7 6. Con el vidrio dominaba el status 4 (3 071) y el 2 era minoría (821): al cielo, el chip reporta sobre todo "sin señal" en vez de "fuera de rango". La cadencia no cambia.
- Exceso de los anómalos sin blanco: mediana 9.8 ms, entre −10.1 y +10.1 ms (vidrio: ±9.5 ms).
- Periodo medio de toda la corrida: 23.293 ms. No se pierde ninguna ranura.
- Con el LED encendido, 36 % de muestras válidas (vidrio: 34 %): igual que antes, la mano entra tarde y se va antes de que acabe la ventana.

### 12.4 Cielo contra vidrio

| | Vidrio (24-oct, intento 3) | Cielo (26-oct) |
|---|---|---|
| Intervalos sin blanco | 3 878 | 3 918 |
| Anómalos sin blanco | 3.3 % | 3.0 % |
| Desfase de los anómalos | ±9.5 ms | ±9.9 ms |
| Saltos a 2 ranuras con blanco | 0 / 1 054 | 0 / 1 022 |
| Válidas con LED apagado > 1 s | 1 / 2 473 | 0 / 2 471 |

El fondo no cambia la cadencia: el régimen sin retorno se comporta igual contra un vidrio fuera de alcance que contra el cielo. El desfase sí cambia un poco (9.5 contra 9.9 ms). Con dos corridas no se sabe si eso viene del fondo, del arranque o de la temperatura.

### 12.5 Hallazgo: dos fases del INT y el cambio de fase en el cruce

`fase_cruces.py` mide la fase de cada interrupción contra sus vecinas, en lugar de solo contar intervalos. Sale lo mismo en las dos corridas:

1. **Sin blanco, el INT lleva una fase adelantada ~9.9 ms** (vidrio: 9.5 ms) respecto a la fase con blanco. En los 29 cruces de hoy, la primera muestra válida llega +10.0 ms (mediana) tarde respecto a las 10 muestras anteriores y a tiempo (+0.0 ms) respecto a las 10 posteriores. Con el vidrio: 30 de 31 cruces, +9.5 ms.
2. **Los anómalos sin blanco son casi todos una sola interrupción que vuelve un instante a la fase "con blanco".** De los 60 intervalos largos (+9.9 ms) sin blanco, 51 van seguidos inmediatamente por uno corto (vidrio: 48 de 60). Es una muestra tardía, no dos anómalos independientes. 53 de las 60 muestras tardías tienen status 2 (vidrio: 48 de 50); las otras 7 son el último cruce, que quedó abierto al terminar la corrida (artefacto de la ventana de vecinas).
3. Esto **corrige la lectura de la sección 11.4**. Ahí decía que los anómalos eran el INT "adelantándose y quedándose adelantado". La fase sin blanco sí es la adelantada, pero los anómalos son vueltas momentáneas a la fase con blanco. El conteo de "cortos y largos casi nunca seguidos" miraba el orden − +, y el par real es + −.

Lectura (hipótesis **sin verificar**, la misma de la 9.2): sin retorno la medición aborta ~9.9 ms antes de completar el budget y el INT sale antes. Con blanco la medición se completa. Una muestra status 2 que alcanza a completarse sale en la fase larga.

**Por qué importa para el presupuesto de error.** El intervalo que termina en la primera detección (de la última muestra sin blanco a la primera con blanco) dura ~23.3 + 9.9 ≈ 33.2 ms **en cada cruce**, no en el 3 % de las veces. El modelo de "ventanas de ancho variable" de la 9.2 suponía que el intervalo largo era la excepción (13.7 % a IMP 20). Si la ventana de cuantización de la detección fuera ese intervalo completo, una uniforme de 33.2 ms da σ ≈ 9.6 ms por sensor, contra 6.73 ms. Eso **no está medido**: depende de en qué momento dentro de la medición el chip "ve" el blanco. El Experimento 1 (dos sensores, misma puerta) lo mide directamente, y su predicción ya escrita (9.5 / 10.7 ms) no se ajusta.

### 12.6 Incidencias

| Síntoma | Causa raíz | Solución |
|---|---|---|
| Intento 1: el logger cortó a los pocos segundos con 8 líneas de ruido | El runner agregó `--idle-s 30`, pero el sketch no imprime nada durante los ~116 s de captura | Repetir sin `--idle-s`. Al abrir el puerto otra vez, la placa se reinició en frío. El intento 1 se conserva |
| `analizar_cielo.py` da 16 % de válidas con el LED apagado | Incluye el primer segundo tras apagarse el LED (la mano saliendo) | Ya resuelto en la sección 11.6: el criterio del fondo usa solo las muestras de más de 1 s después (0 / 2 471) |
