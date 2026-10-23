# Contexto para el Experimento 0b — Jitter con el hub sirviendo la página web
 
> Documento de arranque para un chat nuevo. Léelo completo antes de proponer código.
> Archivos del proyecto que lo acompañan: `DASH_Documento_Maestro_v2.docx`, `experimento0_resultados.md`, `prueba_0_A.ino`, `prueba_0_B.ino`.
> Nota: la descripción del proyecto en claude.ai está desactualizada (habla de 3 sensores). Manda el Documento Maestro v2.
 
---
 
## 1. El proyecto en una página
 
DASH Timing System: cronometraje electrónico del dash de 40 yardas para una liga juvenil de futbol americano en CDMX. Un solo desarrollador, proyecto de servicio social y portafolio.
 
- **4 dispositivos idénticos** (ESP32-WROOM-32U/UE + antena SMA + sensor ToF VL53L1X + batería 18650), en las marcas de **0, 10, 20 y 40 yd**. Un solo diseño de PCB; el rol se elige con jumpers.
- **No hay un quinto ESP32.** El nodo de **40 yd es a la vez la puerta de meta y el hub**: recibe los timestamps de las otras puertas por ESP-NOW, calcula los splits, levanta un **SoftAP** y **sirve la página web** al teléfono del coach.
- **Timestamp distribuido (ADR-003):** cada nodo marca el evento con su propio reloj en la ISR; el hub aplica el offset de reloj. La latencia del radio no entra en la medición.
- **Resync:** ping-pong de 4 timestamps por ESP-NOW cada 30 s, ráfaga de 7 pings separados 20 ms, se conserva el de menor round-trip (filtro de latencia mínima).
- **La nube nunca está en la ruta de medición.** Es una capa opcional posterior (sincronización manual fuera de sesión).
### Nomenclatura (importante, ya causó confusión)
 
| En el Experimento 0 | En el sistema final | Qué hace |
|---|---|---|
| Placa **A** (`prueba_0_A.ino`) | **Hub** = nodo de **40 yd** | Genera el pulso de prueba, hace el resync, recibe el timestamp del otro nodo, calcula el resultado. En el 0b además sirve la página. |
| Placa **B** (`prueba_0_B.ino`) | **Nodo** = cualquier puerta (0, 10 o 20 yd) | Captura su evento y lo reporta. Responde pings del resync. |
 
En el 0b y su reporte se usan los nombres **hub** y **nodo**, no A y B.
 
---
 
## 2. Lo que ya se midió (Experimento 0, aprobado)
 
Montaje: GPIO4 del hub → resistencia 330 Ω → GPIO16 del hub y GPIO16 del nodo. Tierra común obligatoria. Pulso cada 500 ms, 200 eventos, lockout 50 ms, ISR con timestamp como primera instrucción. Cableado verificado con analizador lógico.
 
| Métrica | Valor |
|---|---|
| **σ total** | **168 µs** (REQ-NF-02 pide ≤ 5 ms → margen 30×) |
| Media | −311 µs (artefacto del diente de sierra, no sesgo) |
| Jitter puro de captura | 1.3 µs |
| Deriva de cristal | 11.4–14.7 µs/s (diente de sierra, resync en seq 60/120/180) |
| Piso físico de round-trip F | 2 332 µs |
| Round-trip filtrado (mediana) | 2 483 µs → cota de error ±75 µs |
| Eventos | 200/200, cero pérdidas |
| Distribución | ~uniforme (dominada por la rampa de deriva) |
 
Incidencias ya resueltas (no repetirlas): flancos espurios en el arranque → handshake (tipo 4) antes de adjuntar ISR; σ de 1.8 ms → filtro de latencia mínima; basura en serial → 921 600 baud + impresión diferida al final; hipótesis de interferencia RF → descartada con datos.
 
Detalles del firmware actual: Arduino-ESP32 **core 3.x** (el callback usa `esp_now_recv_info_t`). MAC hub `A4:F0:0F:64:AC:68`, MAC nodo `B0:CB:D8:CA:11:F0`. Ambos en `WIFI_STA`, peer con `channel = 0`. Salida CSV: `seq,resultado_us,offset_us`. Fórmula: `resultado = tHub − (tNodo − offset)`.
 
---
 
## 3. Experimento 0b — qué se quiere probar
 
**Pregunta:** ¿el hub sigue midiendo igual de bien cuando además tiene el SoftAP activo, uno o varios teléfonos conectados y un WebSocket empujando resultados?
 
**Por qué importa:** el Experimento 0 se hizo con el radio solo dedicado a ESP-NOW. En el sistema real, el nodo de 40 yd comparte el radio y el CPU entre ESP-NOW, el resync, el SoftAP y el servidor HTTP. Si el tráfico WiFi retrasa los pongs o el callback, el filtro de latencia mínima elegirá peores muestras y el offset se degradará. Si sale bien, el diseño de 4 nodos queda validado con datos; si sale mal, hay argumento medido para el aislamiento de núcleos (Documento Maestro 13.2) o para un hub dedicado.
 
**No se necesita la página final.** Lo que se prueba es la **carga**, no el diseño visual (las animaciones corren en el teléfono, no en el ESP32). Basta una página de prueba pesada y un WebSocket.
 
### 3.1 Escenarios (200 pulsos cada uno, mismo día, mismo montaje)
 
| ID | Condición del hub |
|---|---|
| C0 | Control: firmware 0b con SoftAP **apagado** (debe reproducir ~168 µs; valida que el cambio de firmware no rompió nada) |
| S1 | SoftAP encendido, sin clientes |
| S2 | 1 teléfono conectado con la página abierta y WebSocket recibiendo cada resultado |
| S3 | Peor caso: 1 teléfono recargando la página completa cada 1 s + WebSocket, y 1–2 teléfonos más conectados escuchando |
 
### 3.2 Qué registrar
 
Extender el CSV a: `seq,resultado_us,offset_us,rtt_us,pongs_recibidos`, donde `rtt_us` es el round-trip del ping elegido en el resync vigente y `pongs_recibidos` cuántos de los 7 llegaron. Además, al final de cada corrida, imprimir la lista de todos los round-trips de todas las ráfagas (para ver si sube el piso F).
 
### 3.3 Criterios de éxito
 
- **Sin degradación relevante:** σ de S3 ≤ ~1.5× la de C0 (≈ 250 µs) y cero eventos perdidos.
- **Siempre obligatorio:** σ ≤ 5 ms (REQ-NF-02).
- **Señal temprana a vigilar:** si la mediana del round-trip elegido sube notablemente sobre ~2.5 ms, o bajan los pongs recibidos, el WiFi está afectando el resync aunque la σ todavía no lo muestre.
---
 
## 4. Cambios de firmware necesarios (mínimos)
 
Regla: **no tocar el algoritmo de resync, la ISR ni la fórmula.** Solo agregar la carga y el registro extra.
 
**Hub (`prueba_0b_hub.ino`, a partir de `prueba_0_A.ino`):**
- `WiFi.mode(WIFI_AP_STA)` y `WiFi.softAP(ssid, password, CANAL, ...)` con **canal fijo** (p. ej. 1). Un flag para apagar el SoftAP en el escenario C0.
- Peer ESP-NOW con `channel = CANAL` explícito.
- Servidor web asíncrono (ESPAsyncWebServer + AsyncTCP; verificar la versión mantenida compatible con core 3.x) con:
  - una página de prueba de tamaño realista (unos cientos de KB, idealmente servida comprimida con gzip desde LittleFS o PROGMEM),
  - un endpoint WebSocket que manda un JSON pequeño por cada evento procesado,
  - en la página, un modo "estrés" que recarga cada 1 s (para S3).
- El envío por WebSocket se hace **en `loop()`**, nunca en la ISR ni en el callback de ESP-NOW.
- Mantener la impresión serial diferida al final de la corrida.
**Nodo (`prueba_0b_nodo.ino`, a partir de `prueba_0_B.ino`):**
- Fijar el radio al mismo canal que el SoftAP del hub (p. ej. `esp_wifi_set_channel(CANAL, WIFI_SECOND_CHAN_NONE)` después de `WiFi.mode(WIFI_STA)`; verificar en la práctica que el canal quedó fijo).
- Peer con `channel = CANAL`.
### 4.1 Trampas conocidas a revisar
 
1. **Canal:** al levantar el SoftAP, el radio del hub se va al canal del SoftAP. Si el nodo está en otro canal, ESP-NOW deja de funcionar en silencio. Síntoma: no hay pongs y el handshake nunca completa.
2. **MAC del hub en modo AP_STA:** la interfaz AP tiene una MAC distinta de la STA (normalmente la STA + 1). El nodo tiene hardcodeada la MAC STA del hub. Mantener el peer del hub en la interfaz STA (`ifidx = WIFI_IF_STA`) y confirmar que los paquetes llegan; si no, imprimir ambas MAC del hub al arrancar.
3. **`delay()` dentro de `hacerResync()`** bloquea `loop()` unos 200 ms por ráfaga. Con servidor asíncrono no debería afectar la web, pero el WebSocket de ese evento saldrá tarde (no afecta la medición).
4. **Repetir C0 el mismo día:** compara contra el control del día, no solo contra los 168 µs históricos.
---
 
## 5. Análisis y entregables
 
- 4 CSV (`exp0b_C0.csv`, `exp0b_S1.csv`, `exp0b_S2.csv`, `exp0b_S3.csv`).
- Reutilizar `graficar.py`: serie temporal y histograma por escenario, y una tabla comparativa (σ, media, eventos perdidos, mediana de rtt elegido, pongs promedio).
- Gráfica extra: distribución de round-trips del resync por escenario (es la evidencia de si el WiFi sube el piso F).
- Sección nueva en `experimento0_resultados.md` (o reporte propio) con la conclusión: el hub puede o no puede servir la página sin afectar la medición, con el número.
## 6. Cómo trabajar en el chat nuevo
 
1. Leer este documento, `prueba_0_A.ino`, `prueba_0_B.ino` y `experimento0_resultados.md`.
2. Escribir primero el firmware del hub y del nodo para el 0b, con cambios mínimos y comentados.
3. Escribir la página de prueba y el procedimiento paso a paso de los cuatro escenarios.
4. Después de correrlo, analizar los CSV y actualizar el reporte.
