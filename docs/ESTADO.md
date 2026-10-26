# Estado del proyecto — dónde vamos y qué sigue

> Única fuente de estado del día a día. Se actualiza al cerrar cada tarea (lo hace la skill `documentar` o se pide "actualiza el estado").
> El Documento Maestro manda sobre la arquitectura; este archivo solo dice en qué paso vamos.
> Cada tarea tiene un **criterio de cierre** medible: si no se cumple, la tarea no está cerrada.

**Última actualización:** 24-oct-2026 (cierre de sesión)

## Cómo retomar (leer primero en un chat nuevo)

Dónde quedó todo al 24-oct-2026:
- **Hecho y documentado:** Exp. 0 y 0b, sensor (barridos y corrida larga con fondo de vidrio), Documento Maestro v2.2, página del coach (W1), guía del esquemático v2, ADR-013.
- **El usuario está haciendo:** el esquemático en KiCad (H1) con `hardware/guia_esquematico/guia_esquematico.pdf`.
- **Lo siguiente en banco, en este orden:**
  1. **S2b**: corrida al cielo abierto. Ya está todo listo: sketch, LED y análisis.
  2. **E1**: riel de 3.3 V con osciloscopio.
  3. **Experimento 1**: necesita un segundo sensor cableado al nodo.
- **Decisiones del usuario pendientes:** reintento de ráfagas (ADR-012), NTC de la celda, cotización JLCPCB (H2). Ver "Decisiones abiertas".
- **El Maestro no se actualiza hasta después de S2b** (pedido del usuario). Los cambios para la v2.3 están juntos abajo.

Cómo correr lo que ya existe:
- **Página del coach sin placas:** `python3 -m http.server 8000 -d webapp` y abrir `http://localhost:8000/index.html?sim=1&rapido=1`. Prueba automática: `.venv/bin/python webapp/probar.py` (Playwright ya instalado en el `.venv`).
- **S2b:** el runner flashea `experimentos/sensor_vl53l1x/corrida_cielo/` en el hub y captura con `--fin "^# fin$" --max-s 400` a `corrida_cielo_IMP25_<fecha>.txt`. Después: `python3 experimentos/sensor_vl53l1x/analizar_cielo.py <archivo>`. Antes de correr, registrar aquí la predicción (o que no la hay).
- **Regenerar el PDF de la guía:** `.venv/bin/python hardware/guia_esquematico/a_pdf.py`.

## Ahora

| # | Tarea | Criterio de cierre | Estado |
|---|---|---|---|
| S2b | Repetir la corrida larga a IMP 25 apuntando al **cielo abierto** (mismo sketch, LED externo en GPIO18) | Mismos criterios que S2: ≥ 1000 intervalos inválidos, 0 saltos a 46.6 ms en válido, < 1 % válidas con LED apagado > 1 s; comparar % y forma de anómalos contra el vidrio (3.3 %, ±9.5 ms) | En corrida (26-oct-2026). **Predicción: el usuario no hace predicción** (registrado el 26-oct-2026, antes de correr). Logger con `--max-s 400` |
| H1 | Esquemático de la placa de nodo en KiCad (`hardware/dash_nodo/`); lo dibuja el usuario con la guía v2 (`hardware/guia_esquematico/guia_esquematico.pdf`, ADR-013) | ERC sin errores y revisión contra "Cómo lo revisamos" (sección 9 de la guía) | En curso (usuario) |

## Siguiente

| # | Tarea | Depende de | Criterio de cierre |
|---|---|---|---|
| E1 | Riel de 3.3 V del hub con osciloscopio bajo carga S3 (guía, sección 8) | Hub del 0b en modo ráfagas + teléfonos; batería y módulo buck-boost para la corrida 4 | Vmin de 3V3 medido en las 4 corridas; criterio propuesto ≥ 3.1 V con batería; predicción escrita antes |
| W2 | Embeber `webapp/index.html` en el hub (gzip en PROGMEM o LittleFS) e implementar PROTOCOLO.md en el firmware | Firmware de producto (o el hub del 0b como banco) | La página carga desde el teléfono en el SoftAP sin datos móviles; `probar.py` adaptado contra el hub real pasa |
| H2 | Códigos LCSC de cada pieza y cotización de 5 placas ensambladas (JLCPCB) | H1 | Costo real contra los $600 MXN del Maestro 15.2; piezas sin stock sustituidas |
| S3 | Experimento 1 (dos sensores, misma puerta) | S2b | σ_exp1 medida contra la predicción escrita (9.5 / 10.7 ms) |

## Después

Experimento 1 → Experimento 2 (sol y alcance) → ruteo y fabricación de la PCB (cuando estén E1, H2 y el Exp. 2) → firmware de producto (binario único, rol por jumpers) → embeber la webapp (W2) → Experimento 3 (gabinete) → Experimento 4 (campo) → nube.

## Decisiones abiertas

- **¿IMP = 12 ms en lugar de 25 ms?** (hallazgo del v5, ver abajo). IMP 12 da 22.41 ms de periodo (2 ranuras) contra 23.31 ms de IMP 25: 0.9 ms menos, lo que baja la cuantización por sensor de 6.73 a 6.47 ms (−4 %). A cambio, su margen hacia el siguiente salto es menor (≥ 0 ms contra ≥ 0.94 ms de IMP 25). **Recomendación: quedarse en IMP 25.** La corrida larga (vidrio) dio 0 saltos a 2 ranuras en 1 054 intervalos con blanco; la ganancia de IMP 12 no mueve el presupuesto de error. Es decisión del usuario.
- **Predicción del Exp. 0b (escribirla ANTES de la primera corrida; no se ajusta después):** **el usuario no hace predicción** (registrado el 23-oct-2026, antes de la primera corrida). Rigen los criterios del plan (3.3). Prueba de ráfagas (agregada tras ver C0–S3, ver `experimentos/exp0b/procedimiento.md`): también **sin predicción**, registrado antes de correrla. Referencias: C0 de hoy ≈ repetición del Exp. 0 (σ = 87 µs, rtt filtrado del Exp. 0: mediana 2 483 µs, piso F = 2 332 µs).
- **¿ADR de reintento de ráfagas?** (conclusión 4 del Exp. 0b). Descartar y repetir una ráfaga cuyo mejor rtt pase de ~3 ms. En las ráfagas ≤ 3 ms el error del offset es σ 58 µs (C0) / 93 µs (S3); en las > 3 ms, 669 / 369 µs. Ataca el mecanismo dominante con o sin WiFi y es más barato que aislar núcleos. Falta: decidir el umbral y cuántos reintentos; medir con la prueba de ráfagas. Es decisión del usuario.
- **Energía del hub (Exp. 0b):** se mide en E1. 422 reinicios por brownout bajo la carga de S3 alimentado por USB; causa física sin aislar (puerto, cable o fuente). Sube la prioridad de las capturas del riel de 3.3 V (DM 20) y del margen del regulador en la PCB; repetir con batería 18650.
- **Predicción de S2 (corrida al cielo, IMP 25): el usuario no hace predicción** (registrado el 24-oct-2026, antes de correr). Fondo: cortina a ~2 m (más allá del alcance del modo corto, 1.38 m). Referencia: a IMP 20 el régimen inválido dio 13.7 % de anómalos; en régimen válido a IMP 25, 0 saltos en 51 + 119 intervalos.
- **Idea anotada, fuera de alcance (Maestro 4.1):** recomendaciones por reglas sobre los splits (sección 14.1) en la capa de nube. El Maestro pone la interpretación automática del desempeño como no-objetivo; requeriría un ADR. Los datos llegarían a la nube por el teléfono como puente (descarga de `/api/sesion`) o por el hub como cliente WiFi; ambos usan el formato de `webapp/PROTOCOLO.md`.
- **ADR-013 (24-oct-2026, aceptado): módulo WROOM-32UE soldado y placa ensamblada por el fabricante** (`docs/adr/ADR-013-modulo-soldado-pcba.md`). Reemplaza la placa portadora con la tarjeta en headers del Maestro 12.1. Un solo USB-C para cargar y programar resuelve la trampa de las dos fuentes en 3V3. ADR-012 queda reservado para el reintento de ráfagas. Pendientes: H2 (códigos LCSC y cotización de 5 placas ensambladas) y decisión sobre el NTC de la celda (TEMP del TP4056: NTC de 10 kΩ en la celda o a GND).
- **Cambios pendientes para el Documento Maestro v2.3** (se juntan para una sola revisión):
  - 9.2 y 0.3 (esperar a S2b; el usuario pidió no actualizar el Maestro todavía): la corrida larga a IMP 25 con fondo de vidrio a ~2 m da: 0 saltos a 46.6 ms en 1054 intervalos válidos. La nota provisional de la 9.2 se cierra en su punto 1.
  - 9.2: en régimen inválido a IMP 25, 3.3 % de intervalos anómalos (no 13.7 %), y son de fase: el INT se adelanta ~9.5 ms sin blanco y regresa al entrar uno, sin perder ranuras. El modelo de "ventanas de ancho variable" (σ 7.54 ms, predicción del Exp. 1 de 10.7 ms) se escribió para intervalos +9.3 ms: hay que revisarlo. Hipótesis sin verificar: sin retorno la medición aborta antes (como ya dice la 9.2 para IMP 20) y el INT sale antes.
  - 12.1, 12.2, 12.5, 15.1, 15.2 y 18: ADR-013 (módulo soldado, chips en lugar de módulos, USB-C único con CH340C, ensamblado por el fabricante). Costo de PCB pendiente de cotización.
  - 16.3: en el cruce (transición) el 47 % de los intervalos lleva el +9.5 ms; el Exp. 1 medirá si eso agrega error al instante del cruce.

## Hecho

| Fecha | Qué | Evidencia |
|---|---|---|
| 20-sep-2026 | Experimento 0 aprobado: σ = 168 µs | `experimentos/exp0/experimento0_resultados.md` |
| 11-oct-2026 | Smoke test y barridos v3/v4 del VL53L1X | Documento Maestro 9.2; ahora resumido en `experimentos/sensor_vl53l1x/sensor_vl53l1x_resultados.md` |
| 23-oct-2026 | Repo reorganizado, CLAUDE.md alineado | commits `a1c3931`, `b705d4e` |
| 23-oct-2026 | I1: scripts de banco funcionando desde WSL2 (compilar, flashear, identificar por MAC, logger, resumen) | `tools/`; COM7 = hub; resumen reproduce σ = 168 µs |
| 23-oct-2026 | **I2: runner cerrado.** Repitió el Exp. 0 completo: 200/200 eventos, σ = 87 µs, media −174 µs (original 168 / −311 µs). Deriva 9.4–10.6 µs/s (original 11.4–14.7): menos deriva, menos diente de sierra | `experimentos/exp0/exp0_repeticion_2026-10-23.csv`; intento 1 fallido por orden de flasheo en `exp0_repeticion_intento1_orden_flasheo_2026-10-23.csv` |
| 23-oct-2026 | I3: skill `documentar`; primer uso: reporte del sensor | `experimentos/sensor_vl53l1x/sensor_vl53l1x_resultados.md` |
| 23-oct-2026 | I4: hooks `proteger_datos` y `revisar_webapp` y permisos del proyecto | 13/13 casos de prueba correctos; el hook bloqueó en vivo un comando real |
| 24-oct-2026 | **W1: prototipo de la página del coach.** En vivo (franja de cancha con las puertas a su distancia real), Sesión, Atletas (roster en el hub) y Ajustes (distancias, CSV). Contrato hub ↔ página en `webapp/PROTOCOLO.md`. 26.9 KB, **8.7 KB con gzip** (la del 0b: 237 KB), sin recursos externos. `probar.py`: 23 comprobaciones con hub simulado, todas OK; resultado en pantalla 1–8 ms después de llegar | `webapp/` |
| 24-oct-2026 | **I5 cerrada; S2 con fondo de vidrio (intento 3): corrida larga a IMP 25 con LED de cruce, ADR-011 se sostiene.** Fondo: vidrio a ~2 m (status 4 fuera de rango), no cielo. 4995/5000 muestras en 116 s (el logger cortó a 300 s imprimiendo las últimas 5), 31 cruces. **Régimen válido: 1054 intervalos, 0 anómalos, 0 saltos a 46.6 ms.** Fondo limpio: 1/2473 válidas con el LED apagado hace > 1 s (las válidas del primer segundo son la mano saliendo, ~40 cm). Régimen inválido: 3878 intervalos, 3.3 % anómalos (intento 1: 8.8 %; smoke test a IMP 20: 13.7 %, solo largos). Los anómalos no son ranuras perdidas: 78 cortos (−9.5 ms) y 78 largos (+9.5 ms), casi nunca seguidos; sin blanco el INT se adelanta ~9.5 ms y regresa a su fase al entrar un blanco (transiciones: 47 % anómalos, mediana +9.5 ms). Periodo medio 23.284 ms | `experimentos/sensor_vl53l1x/corrida_cielo_IMP25_2026-10-24_3.txt`; intento 2 cortado a mano (GND del LED) sin muestras: `…_2.txt` |
| 24-oct-2026 | **I5 + S2, intento 1: NO VÁLIDO como medición.** Sin referencia visible: el LED integrado (GPIO2) no se vio parpadear y los cruces fueron pocos e irregulares. Solo 12.7 % de las muestras fue válido (633/5000), así que no se puede distinguir cruces de lecturas intermitentes de la cortina. Los números (0 saltos, 8.8 % anómalos de ±9.5 ms) quedan como **observación sin confirmar**, no como resultado. Se repite con LED externo en GPIO18 que se enciende 1 s cada 4 s y columna `cue` | `experimentos/sensor_vl53l1x/corrida_cielo_IMP25_2026-10-24.txt` |
| 24-oct-2026 | v2.1 del Documento Maestro movida a `docs/history/` (autorizado por el usuario) | `git mv` |
| 24-oct-2026 | Documento Maestro v2.2: barrido v5, Exp. 0b (nueva 16.2; Exps. 1–4 pasan a 16.3–16.6), margen real de IMP 25, riesgos y ADR candidato | `docs/DASH_Documento_Maestro_v2.2.docx` (0.3 lista los cambios) |
| 24-oct-2026 | **T1: Experimento 0b — HALLAZGO.** σ(S3) 488 µs (REQ-NF-02 con 10×), 0 pérdidas en 1 200 pulsos. Criterio 3.3 no se cumple, pero la σ de 4 ráfagas no es repetible (C0: 263 y 69 µs). Prueba de ráfagas: lentas 14 % → 30 % con carga (p = 0.010), piso F sin cambio, error del offset dentro de la cota 200/200 | `experimentos/exp0b/experimento0b_resultados.md` |
| 23-oct-2026 | **S1: barrido v5 en arranque en frío. IMP 25 → 1.001 ranuras (23.31 ms). ADR-011 se sostiene.** | `experimentos/sensor_vl53l1x/barrido_v5_frio_2026-10-23.txt` |

### Detalle de S1 (barrido v5, 23-oct-2026)

Placa hub (COM7), cartulina fija a 1105 mm, budget 20 ms, 120 muestras por configuración, un arranque en frío por configuración.

**Predicción escrita antes de correr:** el usuario no hizo predicción numérica; la evidencia previa apuntaba a 2 ranuras. **Resultado: 1 ranura.** La hipótesis de que "en frío todo da 2 ranuras" queda refutada.

| IMP (ms) | Ranura (ms) | Periodo (ms) | Ranuras | Jitter (µs) | Anómalos |
|---|---|---|---|---|---|
| 10 | 9.31 | 28.011 | 3.009 | 265.8 | 0/119 |
| 11 | 10.24 | 30.804 | 3.008 | 175.9 | 0/119 |
| 12 | 11.17 | **22.405** | 2.006 | 64.7 | 0/119 |
| 13 | 12.10 | 24.265 | 2.005 | 105.0 | 0/119 |
| 14 | 13.03 | 26.128 | 2.005 | 185.8 | 0/119 |
| 15 | 13.97 | 27.991 | 2.004 | 241.7 | 0/119 |
| 16 | 14.90 | 29.849 | 2.004 | 266.1 | 0/119 |
| 18 | 16.76 | 33.568 | 2.003 | 71.3 | 0/119 |
| 20 | 18.62 | 37.288 | 2.003 | 56.9 | 0/119 |
| 22 | 20.48 | 41.011 | 2.002 | 48.4 | 0/119 |
| 25 | 23.28 | **23.308** | 1.001 | 235.2 | 0/119 |

Lectura:
- El periodo siempre es un número entero de ranuras (IMP × 0.931): el modelo de la 9.2 se confirma en 11 configuraciones.
- El chip usa el **mínimo número de ranuras que alcanza a cubrir la medición**. De ahí sale la duración real T: no cabe en 20.48 ms (IMP 22 da 2 ranuras) y sí cabe en 2 × 11.17 = 22.34 ms (IMP 12).
- 1320/1320 muestras válidas, 0 anómalos, distancia estable (1104–1105 mm): el blanco no se movió.
- Limitación: todo esto es en **régimen válido** (blanco fijo). El régimen inválido (cielo) es S2.

Incidencias resueltas:
- `MAGIC 0xDA5HC001` no es hexadecimal: el v5 no compilaba.
- `RTC_DATA_ATTR` se reinicializa con `ESP.restart()`: el primer intento quedó en un bucle infinito en la config 1. Se cambió a `RTC_NOINIT_ATTR`. Ese intento se conserva en `barrido_v5_intento1_bucle_2026-10-23.txt`: 97 arranques, de los cuales 92 completaron su medición a IMP 10: periodo entre 27.996 y 28.016 ms, σ entre arranques 4.0 µs (corregido por `graficar_sensor.py`; antes aquí decía 97 arranques y hasta 28.006 ms).
