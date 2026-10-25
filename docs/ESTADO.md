# Estado del proyecto — dónde vamos y qué sigue

> Única fuente de estado del día a día. Se actualiza al cerrar cada tarea (lo hace la skill `documentar` o se pide "actualiza el estado").
> El Documento Maestro manda sobre la arquitectura; este archivo solo dice en qué paso vamos.
> Cada tarea tiene un **criterio de cierre** medible: si no se cumple, la tarea no está cerrada.

**Última actualización:** 24-oct-2026

## Ahora

| # | Tarea | Criterio de cierre | Estado |
|---|---|---|---|
| I5 | Sketch para S2 (corrida larga apuntando al cielo) | Compila y registra ≥ 1000 intervalos separados por régimen válido/inválido | **Escrito y compila** (24-oct; IMP 25 y `-DIMP_MS=27`, 25 % de RAM). `experimentos/sensor_vl53l1x/corrida_cielo/` + `analizar_cielo.py` (probado con captura sintética). Falta registrar en placa: se cierra con la primera corrida de S2 |

## Siguiente

| # | Tarea | Depende de | Criterio de cierre |
|---|---|---|---|
| S2 | Corrida larga IMP = 25 ms apuntando al cielo (≥ 1000 muestras, cruces intermitentes) | I5 | Tasa de intervalos anómalos en régimen inválido a IMP 25, y 0 saltos a 46.6 ms en régimen válido |
| W1 | Prototipo de `webapp/` con datos simulados | — (no necesita placas) | Abre en el navegador sin red, ≤ ~20 KB, muestra resultado simulado por WebSocket falso |
| S3 | Experimento 1 (dos sensores, misma puerta) | S2 | σ_exp1 medida contra la predicción escrita (9.5 / 10.7 ms) |

## Después

Esquemático de PCB → firmware de producto (binario único, rol por jumpers) → embeber la webapp (W2) → Experimentos 2–4 → nube.

## Decisiones abiertas

- **¿IMP = 12 ms en lugar de 25 ms?** (hallazgo del v5, ver abajo). IMP 12 da 22.41 ms de periodo (2 ranuras) contra 23.31 ms de IMP 25: 0.9 ms menos, lo que baja la cuantización por sensor de 6.73 a 6.47 ms (−4 %). A cambio, su margen hacia el siguiente salto es menor (≥ 0 ms contra ≥ 0.94 ms de IMP 25). **Recomendación: quedarse en IMP 25** hasta que S2 mida si el margen se agota en régimen inválido; la ganancia de IMP 12 no mueve el presupuesto de error. Es decisión del usuario.
- **Predicción del Exp. 0b (escribirla ANTES de la primera corrida; no se ajusta después):** **el usuario no hace predicción** (registrado el 23-oct-2026, antes de la primera corrida). Rigen los criterios del plan (3.3). Prueba de ráfagas (agregada tras ver C0–S3, ver `experimentos/exp0b/procedimiento.md`): también **sin predicción**, registrado antes de correrla. Referencias: C0 de hoy ≈ repetición del Exp. 0 (σ = 87 µs, rtt filtrado del Exp. 0: mediana 2 483 µs, piso F = 2 332 µs).
- **¿ADR de reintento de ráfagas?** (conclusión 4 del Exp. 0b). Descartar y repetir una ráfaga cuyo mejor rtt pase de ~3 ms. En las ráfagas ≤ 3 ms el error del offset es σ 58 µs (C0) / 93 µs (S3); en las > 3 ms, 669 / 369 µs. Ataca el mecanismo dominante con o sin WiFi y es más barato que aislar núcleos. Falta: decidir el umbral y cuántos reintentos; medir con la prueba de ráfagas. Es decisión del usuario.
- **Energía del hub (Exp. 0b):** 422 reinicios por brownout bajo la carga de S3 alimentado por USB; causa física sin aislar (puerto, cable o fuente). Sube la prioridad de las capturas del riel de 3.3 V (DM 20) y del margen del regulador en la PCB; repetir con batería 18650.
- **Predicción de S2 (corrida al cielo, IMP 25): el usuario no hace predicción** (registrado el 24-oct-2026, antes de correr). Fondo: cortina a ~2 m (más allá del alcance del modo corto, 1.38 m). Referencia: a IMP 20 el régimen inválido dio 13.7 % de anómalos; en régimen válido a IMP 25, 0 saltos en 51 + 119 intervalos.
- **Orden 0b vs webapp:** resuelto: el 0b dice que el hub sí puede servir la página (σ ≤ 488 µs con carga, REQ-NF-02 con 10×).
- **Cambios pendientes para el Documento Maestro v2.3** (no se editan aquí; se juntan para una sola revisión): ninguno por ahora. La v2.2 (24-oct-2026) incorporó el barrido v5 y el Exp. 0b.
- **¿Mover `DASH_Documento_Maestro_v2.1.docx` a `docs/history/`?** La v2.2 la declara SUPERSEDED; sigue en `docs/` hasta que el usuario lo autorice.

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
