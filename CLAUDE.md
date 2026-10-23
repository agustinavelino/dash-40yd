# DASH Timing System — guía para Claude Code

Cronometraje electrónico del dash de 40 yardas para una liga juvenil de futbol americano en CDMX. Un solo desarrollador; proyecto de servicio social y portafolio. Todo se trabaja en **español** (código comentado, commits, documentos).

**Fuente de verdad:** `docs/DASH_Documento_Maestro_v2.1.docx`. Si algo de este archivo contradice al Documento Maestro, manda el Documento Maestro. Cualquier cambio de arquitectura se registra como ADR nuevo y sube la versión del documento; no se cambia en silencio en el código.

Para leer el .docx sin herramientas extra: descomprimir `word/document.xml` con `zipfile` de Python y extraer los `<w:t>`. Secciones más consultadas: 5 (requisitos), 6 (presupuesto de error), 9.2 (timing del sensor), 13 (software), 16 (plan de experimentos), 18 (ADRs), 19 (próximos pasos).

---

## 1. El sistema en breve

- **4 dispositivos idénticos** (ESP32-WROOM-32U/UE + antena SMA + sensor ToF VL53L1X + batería 18650) en las marcas de **0, 10, 20 y 40 yd**. Una sola PCB y un solo binario; el rol se elige con jumpers (ADR-009).
- **No hay quinto ESP32.** El nodo de **40 yd es meta y hub a la vez**: recibe timestamps por ESP-NOW, calcula splits, levanta un **SoftAP** y **sirve la página web** al teléfono del coach.
- **Timestamp distribuido (ADR-003):** cada nodo marca el evento con su propio reloj en la ISR; el hub aplica el offset. La latencia del radio no entra en la medición.
- **Resync:** ping-pong de 4 timestamps por ESP-NOW cada 30 s; ráfaga de 7 pings separados 20 ms; se conserva el de **menor round-trip** y se descartan los demás (no se promedian).
- **La nube nunca está en la ruta de medición.** Es una capa opcional posterior.

### Nomenclatura

| Experimento 0 | Sistema final | Qué hace |
|---|---|---|
| Placa **A** (`prueba_0_A.ino`) | **Hub** = nodo de 40 yd | Pulso de prueba, resync, recibe timestamps, calcula resultado, sirve la página |
| Placa **B** (`prueba_0_B.ino`) | **Nodo** = puerta de 0/10/20 yd | Captura su evento, lo reporta, responde pings |

En código y reportes nuevos se usa **hub** y **nodo**, no A y B. Los archivos históricos del Exp. 0 conservan A/B; no se renombran.

---

## 2. Estado actual

**El estado vivo está en `docs/ESTADO.md`** (qué se está haciendo, qué sigue, criterio de cierre de cada tarea, decisiones abiertas). Leerlo al empezar una sesión y actualizarlo al cerrar una tarea. Aquí solo va lo estable:

- Experimento 0: **aprobado**, σ = 168 µs (REQ-NF-02 pide ≤ 5 ms).
- Sensor: smoke test y barridos v3/v4 hechos (11-oct-2026); resultados solo en la sección 9.2 del Documento Maestro. El **barrido v5 (arranque en frío) está pendiente** y decide si el ADR-011 (IMP = 25 ms) se sostiene: su evidencia viene del v3, que tenía estado heredado. El v4 no está en el repo.
- Pendientes grandes: Experimento 0b, webapp (prototipo con datos simulados), PCB, firmware de producto.

---

## 3. Estructura del repositorio

Híbrida: la de la sección 13.5 del Documento Maestro para el producto, pero **cada experimento es autocontenido** en `experimentos/<id>/` (sketches + datos crudos + gráficas + scripts + reporte), para que un reporte y su evidencia nunca se separen.

```
docs/
  DASH_Documento_Maestro_v2.1.docx   fuente de verdad
  experimento0b_contexto.md          plan del Exp. 0b
  adr/                               (por crear) ADRs nuevos
  history/                           obsoleto: no borrar, no usar como especificación
                                     (bitacora.md, prueba0.md, algoritmo.py)
experimentos/
  exp0/                              Experimento 0 — APROBADO
    prueba_0_A/prueba_0_A.ino        hub (versión final)
    prueba_0_B/prueba_0_B.ino        nodo (versión final)
    pruebanodoa_crudo/, prueba_nodo_b/, prueba_espnow_a_filtrada/,
    prueba_pulsos_analizadorlogico/  iteraciones previas (historial)
    exp0_datos.csv                   200 eventos: seq,resultado_us,offset_us
    corrida_*.txt, primera_corrida_cruda.txt   salidas seriales crudas
    graficar.py, graficar_p3.py      gráficas 01–04
    experimento0_resultados.md       reporte (FORMATO DE REFERENCIA, ver §9)
  sensor_vl53l1x/
    smoke_test_vl53l1x/
    barrido_config_v3/               estado heredado entre configs
    barrido_config_v5/               arranque en frío, una config por arranque (vigente)
  exp0b/                             (siguiente) prueba_0b_hub/, prueba_0b_nodo/, pagina_prueba/
firmware/      (por crear) binario único con selección de rol
webapp/        (por crear) página de producto embebida en el flash del hub
hardware/      (por crear) PCB, gerbers, BOM, gabinete
tools/         (por crear) logger serial y scripts comunes a varios experimentos
cloud/         (fase posterior)
.claude/       agentes y skills del proyecto (§9)
```

Reglas:
- Un sketch vive en una carpeta con **el mismo nombre que su `.ino`** (arduino-cli lo exige). El `.ino` es solo el punto de entrada: el resto puede ser `.cpp`/`.h` en la misma carpeta o en `src/`, y arduino-cli los compila juntos.
- Experimentos: un `.ino` autocontenido (cambios mínimos y fáciles de comparar contra la versión validada).
- Firmware de producto: `.ino` de pocas líneas y la lógica en `.cpp`/`.h`. La lógica pura (offset, emparejamiento por proximidad, splits) no incluye `Arduino.h`, para poder probarla con `g++` en WSL sin placa.
- Un script que solo sirve a un experimento va en la carpeta del experimento; si lo usan dos o más, se mueve a `tools/`.
- Antes de mover o borrar archivos existentes, preguntar al usuario.

---

## 4. Reglas de firmware que NO se rompen

Cada una viene de un error real ya resuelto o de una medición. No "simplificar" ninguna.

1. **La ISR captura `esp_timer_get_time()` como PRIMERA instrucción** y sale. Dentro de la ISR: nada de I2C, Serial, ESP-NOW, `malloc` ni `delay`. Marcar con `IRAM_ATTR`.
2. **El callback de ESP-NOW toma su timestamp como primera instrucción**, copia el mensaje y sale. El trabajo pesado (WebSocket, Serial, cálculos) va en `loop()` o en una tarea propia.
3. **Handshake de arranque (mensaje tipo 4) antes de adjuntar las ISR.** Sin esto, el nodo cuenta flancos espurios mientras el hub bootea (causó un desfase de exactamente 4 pulsos).
4. **No tocar el algoritmo de resync, la ISR ni la fórmula** `resultado = tHub − (tNodo − offset)` en los experimentos. Solo se agrega carga o registro.
5. **Serial a 921 600 baud con impresión diferida** al final de la corrida. Imprimir durante la corrida corrompe la salida y mete jitter.
6. **Arduino-ESP32 core 3.x:** el callback de recepción es `onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len)`.
7. **Pin INT del VL53L1X:** activo en BAJO y LATCHEADO hasta que `read()` lo limpia.
   - ISR por `FALLING`.
   - `attachInterrupt` **después** de `startContinuous()` y con el pin ya limpio; si no, el sistema se cuelga sin error.
   - Red de seguridad: si pasan > 2 periodos sin INT y el pin está abajo, forzar `read()`.
8. **`sensor.init()` solo una vez** al arrancar. Para reconfigurar: `stopContinuous` → configurar → `startContinuous`.
9. **XSHUT:** se libera dejándolo en alta impedancia (INPUT), **nunca** forzándolo a HIGH (no tolera 5 V, el módulo trae pull-up). Reset: OUTPUT LOW, 10 ms, INPUT, 5 ms.
10. **I2C a 100 kHz** (400 kHz falló en protoboard; provisional, re-medir sobre PCB).
11. **Configuración del sensor:** modo corto, timing budget 20 ms, **IMP = 25 ms** (ranura real 23.28 ms = IMP × 0.931; ADR-011). Con IMP = 20 ms el chip se salta ranuras.
12. **Emparejamiento de eventos entre nodos (Exp. 1 en adelante): por proximidad temporal** (ventana ±100 ms tras aplicar offset), **no por número de secuencia**.

### Hardware de banco

- MAC hub (STA): `A4:F0:0F:64:AC:68` · MAC nodo (STA): `B0:CB:D8:CA:11:F0`
- Exp. 0: GPIO4 del hub → 330 Ω → GPIO16 del hub y GPIO16 del nodo. **Tierra común obligatoria.**
- Sensor: SDA 21, SCL 22, INT 16.

---

## 5. Página web local (`webapp/`)

Requisitos (sección 13.3 y REQ-F-05, REQ-NF-08):

- **Cero dependencias externas.** El teléfono conectado al SoftAP no tiene internet: nada de CDN, frameworks remotos ni fuentes externas. Todo el HTML, CSS y JS vive en el flash del ESP32.
- HTML, CSS y JS planos en **un solo archivo**, del orden de 20 KB (servir comprimido con gzip si conviene).
- Funciones: selección de atleta, armar el sistema, resultado en vivo (visible ≤ 2 s tras cruzar la meta), tabla de la sesión ordenada, configuración de distancias de puertas (ADR-010), descarga de CSV.
- Resultados en vivo por **WebSocket**. Clientes previstos: 1–2 (el SoftAP soporta 4–8).
- Tiempos reportados a **centésimas**; el sistema no distingue 5.10 de 5.12 (sección 6.1).
- Desarrollar y probar en el navegador de la computadora con **datos simulados** antes de embeberla.

Para el Experimento 0b se necesita además una **página de prueba pesada** (cientos de KB) con WebSocket y un **modo estrés que recarga cada 1 s**. Es distinta de la página de producto y vive en `experimentos/exp0b/`, no en `webapp/`; no mezclarlas.

Servidor en el ESP32: ESPAsyncWebServer + AsyncTCP en su versión mantenida (organización ESP32Async en GitHub). **Aún no están instaladas**; verificar compatibilidad con core 3.3.x antes de fijar versión.

---

## 6. Toolchain (medido en esta máquina, 23-oct-2026)

El entorno es **WSL2 sobre Windows**. Las placas USB solo las ve Windows, así que todo lo que toca hardware se ejecuta con binarios de Windows llamados desde WSL.

| Herramienta | Ruta / valor |
|---|---|
| arduino-cli 1.5.1 (viene con el Arduino IDE) | `"/mnt/c/Users/aagus/AppData/Local/Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"` |
| Core | `esp32:esp32` **3.3.11**, FQBN `esp32:esp32:esp32` |
| Carpeta de librerías (Windows) | `C:\Users\aagus\OneDrive\Documents\Arduino\libraries` |
| Librerías instaladas relevantes | `VL53L1X` (Pololu) 1.3.1. Faltan ESPAsyncWebServer y AsyncTCP |
| Python con pyserial (Windows) | `/mnt/c/Users/aagus/AppData/Local/Programs/Python/Python311/python.exe` (pyserial 3.5) |
| Python de WSL | `python3` sin pyserial ni matplotlib (instalar en un venv antes de graficar) |
| esptool 5.3.1 (viene con el core) | `/mnt/c/Users/aagus/AppData/Local/Arduino15/packages/esp32/tools/esptool_py/5.3.1/esptool.exe` |
| Puertos vistos | COM3, COM4, COM11, COM12 (todos "Unknown"; no se sabe cuál es hub o nodo sin leer la MAC) |

Scripts de banco (`tools/`):

| Script | Corre con | Qué hace |
|---|---|---|
| `identificar_puertos.py` | `python3` (WSL) | Lee la MAC de fábrica de cada COM con esptool y dice si es hub o nodo. **Reinicia las placas.** |
| `logger_serial.py` | Python de **Windows** | Guarda el serial crudo a un archivo; termina con el marcador `--fin`, `--max-s` o `--idle-s`. Rutas con `wslpath -w` |
| `resumen_corrida.py` | `python3` (WSL) | σ, media, eventos y pérdidas de un CSV tipo Exp. 0/0b, en 4–5 líneas |

Compilar (verificado, ~20 s): `"$CLI" compile --fqbn esp32:esp32:esp32 "$(wslpath -w <carpeta_del_sketch>)"`.

Notas:
- arduino-cli.exe recibe rutas de Windows. Para compilar un sketch del repo, pasarle la ruta con `wslpath -w <carpeta_del_sketch>`.
- El puerto serial se lee con el Python de Windows (pyserial), no con el de WSL: WSL2 no ve los COM.
- **Logging serial:** script en `tools/` que lea el puerto a 921 600 baud y escriba a un archivo con el nombre del escenario (p. ej. `exp0b_C0.csv`). Arrancar el logger **antes** de la corrida.
- Solo un programa puede tener abierto el puerto. Si el monitor del Arduino IDE está abierto, cerrarlo antes de subir o leer.
- Abrir el puerto normalmente **reinicia** el ESP32.
- Hay dos placas conectadas: **confirmar qué puerto es el hub y cuál el nodo** antes de flashear (leer la MAC que imprime cada placa al arrancar).
- El usuario hace todo lo físico (cableado, teléfonos, blancos del sensor). Avisarle cuándo le toca y esperar su confirmación.

Pendientes conocidos:
- `experimentos/exp0/graficar.py` lee `datos_raw.csv`, pero el archivo se llama `exp0_datos.csv`.

---

## 7. Forma de trabajar

- **Principio rector:** toda decisión técnica lleva un número o un criterio de prueba. Si no, es una preferencia y se marca como tal.
- **Predicciones antes de medir.** Si un experimento tiene predicción escrita, no ajustarla después de ver los datos.
- Distinguir siempre lo **medido** de lo **provisional** o **estimado** (el Documento Maestro tiene una tabla de firmeza de la evidencia).
- Cambios mínimos y comentados en los sketches de experimentos; explicar en el comentario *por qué*, no solo qué.
- Datos crudos (`.csv`) y gráficas de cada experimento se guardan en la carpeta del experimento junto a su reporte.
- **Commits pequeños**, uno por cambio funcional, con mensaje en español que diga qué y por qué. Hacer `push` solo cuando el usuario lo indique.

---

## 8. Documentos de referencia

- `docs/DASH_Documento_Maestro_v2.1.docx` — arquitectura, requisitos, presupuesto de error, ADRs
- `experimentos/exp0/experimento0_resultados.md` — Experimento 0 completo
- `docs/experimento0b_contexto.md` — plan del Experimento 0b (escenarios C0, S1, S2, S3; CSV extendido; criterios de éxito; trampas de canal y MAC con SoftAP)
- `experimentos/exp0/prueba_0_A/`, `prueba_0_B/` — firmware base del hub y del nodo
- `experimentos/sensor_vl53l1x/` — smoke test y barridos del sensor

---

## 9. Herramientas de Claude Code en este proyecto

Estado: **en diseño** (ver el plan acordado el 23-oct-2026). Cuando existan, se documentan aquí.

- **Agente de banco** (`.claude/agents/`): compila, flashea, lee el serial y devuelve un resumen corto (estado, conteos, σ, archivo guardado), no la salida completa, para no saturar el contexto. Antes de flashear identifica cada puerto por su MAC y solo sube si coincide con el rol esperado; si hay duda, se detiene y pregunta.
- **Skill de documentación** (`.claude/skills/`): cuando el usuario pide "documenta" o "haz el reporte", genera el reporte con el formato de `experimentos/exp0/experimento0_resultados.md`: línea de resultado al inicio, objetivo, principio, montaje, procedimiento, resultados con gráficas, contexto en el presupuesto de error, conclusiones numeradas, incidencias resueltas y tabla de archivos.
