# Auditoría de la guía del esquemático v3

- **Qué se auditó:** `hardware/guia_esquematico/guia_esquematico.html` (fuente del PDF), versión 3 del 29-oct-2026, contra el Documento Maestro v2.3 (11.2, 12, 15), el ADR-013 y las hojas de datos.
- **Cuándo:** 9 al 12 de noviembre de 2026. El proyecto de KiCad (`hardware/dash_nodo/`) todavía estaba vacío: se auditó el plan, no un esquemático dibujado.
- **Resultado:** la arquitectura se sostiene. Se encontraron 5 errores o huecos reales en la guía, 2 contradicciones con el Maestro y 4 decisiones de diseño. Todo lo aplicable ya está en la **guía v4** (12-nov-2026) y en el **ADR-014**.
- **Método:** cada hallazgo se verificó contra una fuente antes de documentarlo. Los que no se sostuvieron se dejan abajo, marcados como retirados, para que no se vuelvan a levantar.

## 1. Hallazgos confirmados

| # | Hallazgo | Evidencia | Qué se hizo en la v4 |
|---|---|---|---|
| H1 | **Al TPS63021 le faltaban pines y los capacitores no cumplían la hoja de datos.** La guía no decía qué hacer con PS/SYNC, VINA ni PG, y sugería 10 µF a la salida | TI SLVS916I (rev. I, oct-2019). Tabla de pines: PS/SYNC *"must not be left open"*; PG *"can be left open"*. 8.2.2.5: VINA con 0.1 µF, *"must not be higher than 0.22 µF"*. 8.2.2.3: *"recommended nominal output capacitors are three times 22 µF"*; la tabla 1 solo admite 2×22, 3×22, 4×22 o ≥ 100 µF con 1.5 µH. 8.2.2.4: 10 µF a la entrada y *"if the input supply is located more than a few inches"*, ~47 µF adicionales (aquí la celda llega por cables y pasa por el interruptor). 8.2.2.3: *"no upper limit for the output capacitance"*: los 470 µF del bulk son válidos | PS/SYNC a VIN (modo PWM fijo), VINA a VIN con 100 nF, PG al aire, salida 3 × 22 µF, entrada 2 × 10 µF + 47 µF |
| H2 | **R4 y R14 "sin soldar en la primera energización" no es compatible con el ensamblado de fábrica.** La guía les daba código LCSC y no los incluía en la lista de piezas sin poblar (A8) | JLC solda todo lo que viene en la BOM y ensambla las 5 placas iguales (ADR-013) | R4 y R14 pasan a DNP y se sueldan a mano (1206) después de probar cada bloque. Junto a R4 van pads para medir corriente con un amperímetro en serie |
| H3 | **Designadores repetidos.** R23 y R24 eran el NTC (hoja 2) y también el LED y el buzzer (hoja 5) | Texto de la guía v3, secciones 5 y 7 | Interfaz renumerada (R25, R26) |
| H4 | **La carga no termina en el uso normal de banco.** El TP4056 no separa la carga de la celda del consumo del sistema y no tiene temporizador. Para programar, el interruptor tiene que estar encendido, y el sistema consume ~80–120 mA, más que el corte de fin de carga (C/10 = 58 mA con 580 mA). Además, sin celda no se puede programar | Hoja del TP4056 (terminación a C/10, sin temporizador). El ADR-013 ya lo anotaba como limitación; lo nuevo es que pasa en cada sesión de banco | **ADR-014:** carga compartida con 3 piezas Basic. Aprobado por el usuario el 12-nov |
| H5 | **El Maestro se contradice con la guía sobre la carga.** El Maestro 11.2 y la tabla de riesgos (17) prohíben cargar "con el gabinete cerrado"; la guía v3 dice que el USB-C "queda expuesto en el gabinete" | Maestro v2.3, 11.2 y 17; guía v3, sección 5 | El usuario decidió USB accesible con tapón de TPU y NTC poblado. Cambio propuesto para el Maestro v2.4 (ver sección 4). Confirmar la nueva regla |
| H6 | **Fuga del TVS en estado apagado.** El SMAJ5.0A está siempre conectado a la celda, antes del corte del DW01A, así que también descarga una celda que ya llegó al corte por sobredescarga | LCSC C10758 (RUILON SMAJ5.0A): Ir máxima de **800 µA a 5 V**. A 4.2 V seguramente es mucho menor; no hay dato | Se mide en la primera placa, junto con la fuga del diodo del ADR-014. Criterio propuesto en la guía v4, sección 10 |
| H7 | **Falta una forma de programar si el USB o el CH340C fallan.** Sin ella, una falla en ese bloque deja la placa inservible, y el ADR-013 ya califica de alto el riesgo de no poder rehacer la placa | ADR-013, Consecuencias | Header 1×6 sin poblar (3V3, GND, TXD0, RXD0, EN, IO0) y una resistencia de 1 kΩ entre el TXD del CH340C y RXD0, para que un adaptador externo pueda imponerse sobre el CH340C |
| H8 | **El sensor de temperatura del Maestro no sirve en campo.** El NTC del TP4056 solo funciona con USB conectado, porque su divisor se alimenta de VBUS | Hoja del TP4056; guía v3, sección 5 | NTC de placa a GPIO35 (ADC1). Ver la sección 3 |

### Nota sobre VINA

La figura de la aplicación típica de la revisión I (figura 7) **no dibuja la unión entre VINA y VIN**; el historial de cambios dice *"removed the connection from VINA to VIN"*. Aun así, todas las condiciones de prueba de la hoja usan `VIN = VINA` y el bloqueo por bajo voltaje (UVLO) se mide en VINA. La guía v4 une VINA a VIN y lo marca **PENDIENTE de comparar con el esquemático de la tarjeta de evaluación de TI**.

**Cerrado el 16-nov-2026 (guía v4.2): VINA no se une a VIN.** La tarjeta de evaluación (TI SLVU365, figura 9) deja VINA solo con C8 de 0.1 µF a GND, y el diagrama de bloques del TPS63021 (SLVS916I, figura 4) muestra que VINA ya se alimenta de VIN por una resistencia interna: con el capacitor forma un filtro RC, y un cable externo VINA–VIN se lo salta. `VIN = VINA` en las condiciones de prueba describe el voltaje con que se mide, no un cable. EN y PS/SYNC siguen a VIN, como en la tarjeta de evaluación.

## 2. Hallazgos retirados o rebajados

| Lo que se dijo | Por qué se retira | Estado |
|---|---|---|
| "Faltan PWR_FLAG" | La guía v3 ya decía "en +3V3 si el ERC lo pide" y advertía en A3 que las piezas importadas traen mal el tipo de pin (se comprobó: todos los pines importados con easyeda2kicad son `unspecified`). Solo faltaba 3V3_MOD, y el ERC lo señala de todos modos | Precisión. Se agregaron 3V3_MOD y las redes nuevas a la lista de PWR_FLAG |
| "El bulk está en la red equivocada" | Con R14 soldado, +3V3 y 3V3_MOD son el mismo nodo eléctrico | Claridad de texto. La v4 lo pone en 3V3_MOD, junto al pin VDD |
| "El cable del sensor sale del gabinete: falta protección contra estática y contra cortos" | Salió del Maestro 12.1 ("hacia el gabinete del sensor") y del BOM (8 prensaestopas, cable de 6 hilos). El usuario decidió **un solo gabinete con el sensor adentro**: el cable es interno y corto | Retirado. El Maestro 12.1 y el BOM se corrigen en la v2.4 |
| "580 mA de carga superan los 500 mA de USB 2.0" | Es cierto según la norma, pero casi ninguna laptop lo hace cumplir | Con el ADR-014 el USB alimenta además al sistema; la carga se bajó a 400 mA (R9 = 3 kΩ, decidido el 15-nov; ver el ADR) |
| "Header de expansión" | No es un error, es una sugerencia | Decisión pendiente del usuario |

## 3. Decisiones del usuario (11 y 12 de noviembre de 2026)

| Tema | Decisión | Razón o evidencia |
|---|---|---|
| Pull-ups de I2C | **No se agregan.** Las de la placa siguen dibujadas pero sin poblar | Medido por el usuario: 10 kΩ entre SDA y VIN en el módulo TOF400C. A 100 kHz, con ~50 pF de bus interno, la subida es ≈ 0.85 · 10 kΩ · 50 pF ≈ 0.42 µs, contra 1 µs permitido. A 400 kHz (0.3 µs) haría falta menos de ~35 pF: **hipótesis** de por qué 400 kHz falló en el protoboard (Maestro 12.2), no medida. Nota: en las pruebas también estuvieron activos los pull-ups internos del ESP32 (`Wire.begin()` del core 3.3.11, `esp32-hal-i2c-ng.c:131`, e `INPUT_PULLUP` en el INT), de ~45 kΩ |
| NTC de la celda | **Se pobla** (antes: dibujado y sin poblar, 31-oct) | Criterio del usuario: no es opcional |
| Gabinete | **Uno solo, con el sensor adentro**, impreso en **ASA beige**; el gabinete se diseña alrededor de la PCB | Menos trabajo que una segunda caja; ASA aguanta el sol y el color claro absorbe menos |
| Sensor | **Pieza impresa en cuña** dentro del gabinete, con la ventana paralela al sensor, visera y empaque; cuñas intercambiables de 15°, 20° y 25° | Maestro 8.2 da un rango de inclinación, no un valor; el Exp. 3 elige con datos |
| Entradas (USB, etc.) | **Tapones de TPU** | Lluvia ligera (REQ-NF-06) |
| Ventilación | **Se decide después de medir** la temperatura de la celda al sol (Exp. 2). Orden si hace falta: techito con espacio de aire → persianas hacia abajo → compuertas | La electrónica disipa < 1 W; el sol aporta varios watts sobre las paredes. Un ventilador en caja sellada no saca calor |
| Rol | **DIP de 2 posiciones** adentro, fijado una vez; rótulo y color por fuera | 4 roles = 2 bits. Un selector de 16 posiciones sobra. Un rol en el código rompe el binario único (ADR-009). La asignación desde el hub queda como posible ADR futuro |
| Carga compartida | **Aprobada** | ADR-014 |
| Temperatura de placa | **NTC de 100 kΩ** a GPIO35, sujeto a que haya pieza. Alternativa: NTC de 10 kΩ con el divisor alimentado desde un GPIO | Con 10 kΩ / 10 kΩ el divisor gasta ~165 µA siempre, ~3 veces el consumo dormido estimado; con 100 kΩ, ~16 µA |
| Buzzer | **En las 4 unidades**, más grande que el MLT-5030, con orificio acústico en el gabinete | Montaje de los trípodes (REQ-NF-04). Criterio propuesto: se escucha a 15 m con ruido de entrenamiento. El LED, con su ventanita o guía de luz |
| Agujeros de montaje | Sí | — |
| OTA | **Se evalúa cuando esté definido el firmware del hub.** Lo único que toca el hardware es la flash (N4 contra N8) | La OTA necesita dos copias del programa en la flash |

## 4. Cambios pendientes para el Documento Maestro v2.4

- **12.1:** "Conector polarizado de 6 vías hacia el gabinete del sensor" → conector interno; un solo gabinete.
- **15.1:** quitar o reducir los prensaestopas y el cable de 6 hilos; "Cable Micro USB" → USB-C.
- **11.2, 12.4 y 17:** regla de carga. Propuesta: "no cargar en campo, al sol ni mojado" en lugar de "no cargar con el gabinete cerrado", ahora que el NTC va poblado y la carga termina bien (ADR-014). **Falta la confirmación del usuario.**
- **12.1 y 12.2:** carga compartida (ADR-014); "los 5 V solo van al cargador" cambia.
- **12.1:** buzzer en las 4 unidades; NTC de placa; header de programación.
- **18:** agregar el ADR-014.

## 5. Lo que estaba bien

- Una sola fuente en el riel de 3.3 V. Con el ADR-014 sigue así: el USB alimenta la *entrada* del buck-boost, nunca el riel.
- BAT- separado de GND, con la explicación de por qué.
- Pines de arranque libres, GPIO12 sin pull-up, batería por ADC1, pines del sensor iguales a los validados en banco.
- XSHUT sin pull-up propio; pull-ups opcionales a SENS_VIN (evita alimentar el sensor por sus pines); sensor apagado mientras arranca el ESP32.
- Orientación del MOSFET de polaridad inversa: correcta.
- Ventana del NTC de la celda: ~4 a ~39 °C, recalculada con 6.8 kΩ y B = 3950 (la v3 decía ~3 a ~41 °C).
- Etiquetas de firmeza y ruteo condicionado a E1.

## 6. Fuentes

- Texas Instruments, *TPS63020, TPS63021 High Efficiency Single Inductor Buck-Boost Converter With 4-A Switches*, SLVS916I, revisión de octubre de 2019. <https://www.ti.com/lit/ds/symlink/tps63020.pdf>
- LCSC C10758, SMAJ5.0A (RUILON). <https://www.lcsc.com/product-detail/C10758.html>
- Arduino-ESP32 core 3.3.11, `cores/esp32/esp32-hal-i2c-ng.c`, línea 131.
- `hardware/dash_nodo/lib/dash.kicad_sym` (tipo de pin de las piezas importadas).

## 7. Revisión de la hoja de energía contra las hojas de datos (16-nov-2026, guía v4.3)

Cada pieza se comparó contra la hoja de datos **del fabricante del código LCSC que se compra**, no contra la de otro fabricante con el mismo número de parte. Las conexiones se compararon contra la netlist del archivo de KiCad del usuario (exportada con `kicad-cli`; versión guardada a las 13:41 del 16-nov, que todavía no tenía cableado el buck-boost).

| Pieza (LCSC) | Hoja consultada | Qué se comprobó | Resultado |
|---|---|---|---|
| DW01A (C351410, PUOLOP) | PUOLOP DW01A rev. B, p. 2–4 | Pinout (1 OD, 2 VM = CS en KiCad, 3 OC, 4 TD, 5 VCC, 6 GND); aplicación típica R1 100 Ω, C1 0.1 µF, **R2 1 kΩ**; TD | **Corregido: R3 de 2 kΩ a 1 kΩ.** La hoja de H&M que enlaza KiCad también dice 1 kΩ; los 2 kΩ no salen de ninguna. TD se queda sin conectar (ver abajo) |
| FS8205A (C2830320, TECH PUBLIC) | TECH PUBLIC FS8205A, p. 1–2 | Pinout 1 S1, 2 D, 3 S2, 4 G2, 5 D, 6 G1; VGS ±10 V; RDS(on) ≤ 25 mΩ a 4.5 V y ≤ 31.5 mΩ a 2.5 V | Bien. Con el umbral de sobrecorriente del DW01A (130–170 mV) y dos MOSFET en serie, el corte queda en ~2.3–4.4 A, arriba de los picos de la placa (~0.6 A desde la celda) |
| AO3401A (C15127, AOS) | AOS rev. 3.1 | Pinout SOT-23 (1 G, 2 S, 3 D); VGS ±12 V; RDS(on) < 85 mΩ a −2.5 V | Bien. Q1 y Q7 ven VGS ≈ −VCELDA (≤ 4.3 V) |
| SMAJ5.0A (C10758, RUILON) | RUILON SMAJ, rev. DEC-16 | VRWM 5.0 V, VBR 6.40–7.25 V, VC 9.2 V a 43.5 A, IR ≤ 800 µA a 5 V | Bien. El símbolo de KiCad (hereda de SM6T6V8A) nombra sus pines A1/A2 aunque es unidireccional; el pin 1 es el cátodo (la barra) y en el archivo va a `BATP`, que es lo correcto. VC < 10 V, el máximo de VDD del DW01A |
| SMD1206P150TF (C20802, RUILON) | RUILON SMD1206, A6/2026-03 | Ihold 1.5 A, Itrip 3.0 A, Vmax 6 V, R 0.025–0.13 Ω | Bien. Ihold baja a ~1.17 A a 50 °C y ~1.06 A a 60 °C (tabla de reducción por temperatura) |
| B5819W (C8598, JSCJ) | JSCJ B5817W–B5819W, rev. D | 40 V, 1 A, VF ≤ 0.6 V a 1 A, IR ≤ 1 mA a 40 V | **Corregido el texto:** la guía decía que caía 0.15–0.4 V; cae hasta 0.6 V. Con USB, `VPATH` queda en ~4.4–4.7 V. El umbral de "USB conectado" en `VBAT_ADC` queda pendiente de medir |
| TP4056 (C16581, Top Power) | Top Power rev. 2.4 | CE a VCC; 10 µF en VCC y BAT; LED con 1 kΩ; TEMP 45–80 % de VCC; I = 1100 V / RPROG | Bien en conexiones. **Corregido el texto:** 3 kΩ dan ~370 mA por la fórmula (la tabla dice 400), ±15 %. La hoja trae una errata en los máximos absolutos (BAT −0.3 a 0.7 V; debe ser ~7 V) |
| TPS63021 (C202140, TI) | SLVS916I y SLVU365 | Pinout y almohadilla térmica (pad 15 en la huella importada); VINA; capacitores | Bien con la corrección de VINA de la v4.2 |
| CKCS5040-1.5uH (C5120877, CENKER) | CENKER CKCS | 1.5 µH, Isat 6.3 A, Irms 4.0 A, DCR 18 mΩ | Bien. La hoja lista el 1.5 µH como /N (±30 %); LCSC lo vende como /M. Con la ecuación 2 de TI (2.4 V de entrada, 0.6 A de salida) el pico es ~1.1 A |

### C19: de cerámico a polímero (23-nov-2026, guía v4.4)

La v4.3 ponía el bulk de 47 µF de entrada del TPS63021 como cerámico de 1206, sin justificarlo. La hoja de TI (8.2.2.4) lo pide *"electrolytic or tantalum"*. Con resistencia interna (ESR), el bulk amortigua el rebote LC entre los cables y los cerámicos al cerrar el interruptor o enchufar el USB; además, un cerámico de 47 µF en 1206 pierde más de la mitad de su capacidad a 4–5 V y el polímero no.

- **Tántalo común (MnO2), descartado:** la hoja del CA45 (C136751, sección 9.2.2) pide trabajar a 1/3 del nominal en circuitos de baja impedancia (carga instantánea > 300 mA o subida < 1 ms, como una batería con interruptor) y dice que ~90 % de sus fallas son cortos o fuga. Para 5.5 V harían falta ≥ 16.5 V de nominal, y un corto quedaría directo sobre la celda.
- **Polímero de tántalo, elegido:** Panasonic 10TPE47MAZB (C147768): 47 µF, 10 V, ESR 35 mΩ, caja B (3528), 7 532 piezas en stock el 23-nov, la mayor existencia entre los polímeros de 47 µF / 10 V. Alternativa: KEMET T520B476M010ATE035 (C141441, 4 440 en stock), misma huella y ESR. La hoja del T520 recomienda usarlo hasta el 90 % del nominal (≤ 10 V): 9 V contra 5.5 V como máximo aquí.
- **Lo que no resuelve:** el sobretiro al enchufar el USB con el interruptor apagado depende de C7 (10 µF cerámico en VCC del TP4056) y del cable. Se revisa con la hoja 2 y se mide en la primera placa.

### Hallazgo retirado: "TD del DW01A va conectado a OD"

Otra revisión leyó en la página 3 de la hoja de PUOLOP que TD va unido a OD. No es así: la línea horizontal que pasa por TD, OD, OC y VM es el **contorno del integrado** (sigue por arriba hasta VCC y cierra el rectángulo con la leyenda "DW01A"). Los cables salen hacia afuera: VCC y GND a la izquierda, VM a la derecha, OD y OC hacia abajo. De TD no sale ninguno. Si esa línea fuera un cable, OD, OC y VM estarían en corto y no podrían controlar los dos MOSFET por separado. La tabla de pines describe TD como *"Test pin for reduce delay time"*. El símbolo de KiCad lo marca como `no_connect`. **TD se queda sin conectar.**

### Lo que estas hojas no resuelven

- Fuga real de D1 y D6 a 4.2 V con el sistema apagado (las hojas solo dan el máximo a 5 V y a 40 V): ya está en la sección 10 de la guía.
- `VSYS` con USB conectado, para fijar el umbral de "USB conectado".
- Sobretiro al cerrar el interruptor o al enchufar el USB con capacitores cerámicos a la entrada (máximo absoluto del TPS63021: 7 V). Estimado: la resistencia en serie (PTC, Q1, Q7 y FS8205A, ~0.2–0.3 Ω) amortigua el cierre del interruptor. El enchufe del USB depende del cable. Se mide en la primera placa.
- Temperatura del FS8205A, del PTC y del inductor dentro del gabinete.

Fuentes adicionales: hojas de datos enlazadas desde las páginas de LCSC de cada código de la tabla (consultadas el 16-nov-2026); PUOLOP DW01A en <https://cdn.promelec.ru/upload/items/2024/07/19/PUOLOP-DW01A_.pdf>; H&M DW01A en <https://hmsemi.com/downfile/DW01A.PDF>; TI SLVU365 (TPS63020EVM-487).
