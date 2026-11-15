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

## 2. Hallazgos retirados o rebajados

| Lo que se dijo | Por qué se retira | Estado |
|---|---|---|
| "Faltan PWR_FLAG" | La guía v3 ya decía "en +3V3 si el ERC lo pide" y advertía en A3 que las piezas importadas traen mal el tipo de pin (se comprobó: todos los pines importados con easyeda2kicad son `unspecified`). Solo faltaba 3V3_MOD, y el ERC lo señala de todos modos | Precisión. Se agregaron 3V3_MOD y las redes nuevas a la lista de PWR_FLAG |
| "El bulk está en la red equivocada" | Con R14 soldado, +3V3 y 3V3_MOD son el mismo nodo eléctrico | Claridad de texto. La v4 lo pone en 3V3_MOD, junto al pin VDD |
| "El cable del sensor sale del gabinete: falta protección contra estática y contra cortos" | Salió del Maestro 12.1 ("hacia el gabinete del sensor") y del BOM (8 prensaestopas, cable de 6 hilos). El usuario decidió **un solo gabinete con el sensor adentro**: el cable es interno y corto | Retirado. El Maestro 12.1 y el BOM se corrigen en la v2.4 |
| "580 mA de carga superan los 500 mA de USB 2.0" | Es cierto según la norma, pero casi ninguna laptop lo hace cumplir | Con el ADR-014 el USB alimenta además al sistema; se propone bajar la carga a 400 mA (ver el ADR) |
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
