# Estado del proyecto — dónde vamos y qué sigue

> Única fuente de estado del día a día. Se actualiza al cerrar cada tarea (lo hace la skill `documentar` o se pide "actualiza el estado").
> El Documento Maestro manda sobre la arquitectura; este archivo solo dice en qué paso vamos.
> Cada tarea tiene un **criterio de cierre** medible: si no se cumple, la tarea no está cerrada.

**Última actualización:** 23-oct-2026

## Ahora

| # | Tarea | Criterio de cierre | Estado |
|---|---|---|---|
| I1 | Infraestructura de banco: `tools/identificar_puertos.py`, `logger_serial.py`, `resumen_corrida.py` | `identificar_puertos` dice qué COM es hub y cuál nodo; `resumen_corrida` sobre `exp0_datos.csv` da σ = 168 µs | En curso |
| S1 | **Barrido v5** (arranque en frío) | El logger captura la tabla `RESUMEN DEL BARRIDO EN FRIO` con las 11 configuraciones; decisión sobre ADR-011 escrita (ver abajo) | Listo para correr (ya compila) |

## Siguiente

| # | Tarea | Depende de | Criterio de cierre |
|---|---|---|---|
| I2 | Agente de banco (`.claude/agents/banco.md`) | I1 | Corre el Exp. 0 completo sin intervención en la conversación y devuelve σ ≤ 250 µs con 200/200 eventos |
| I3 | Skill `documentar` | — | Genera el reporte del sensor (smoke test + barridos) con el formato del Exp. 0 |
| I4 | Hooks (datos crudos protegidos, webapp sin URLs externas) y permisos | — | Un intento de editar un `.csv` de `experimentos/` queda bloqueado |
| S2 | Corrida larga IMP apuntando al cielo (≥ 1000 muestras, cruces intermitentes) | S1 | Tasa de intervalos anómalos en régimen inválido medida a la IMP vigente |
| T1 | **Experimento 0b** (C0, S1, S2, S3) | I2 | 4 CSV; σ(S3) ≤ 1.5 × σ(C0) y 0 pérdidas, o hallazgo documentado |
| W1 | Prototipo de `webapp/` con datos simulados | — (no necesita placas) | Abre en el navegador sin red, ≤ ~20 KB, muestra resultado simulado por WebSocket falso |
| S3 | Experimento 1 (dos sensores, misma puerta) | S1, S2 | σ_exp1 medida contra la predicción escrita (9.5 / 10.7 ms, o la que resulte de S1) |

## Después

Esquemático de PCB → firmware de producto (binario único, rol por jumpers) → embeber la webapp (W2) → Experimentos 2–4 → nube.

## Decisiones abiertas

- **ADR-011 (IMP = 25 ms) depende del v5.** El ADR se validó con 51 muestras del barrido v3, que tenía estado heredado. En arranque en frío (smoke test y v4) todo dio 2 ranuras. En producción cada nodo arranca en frío.
  - Si v5 da **1 ranura** a IMP 25 → ADR-011 se sostiene; seguir a S2.
  - Si da **2 ranuras** (~46.6 ms) → la cuantización por sensor sube de 6.72 a ~13.4 ms, la predicción del Exp. 1 cambia y hace falta un ADR nuevo y la v2.2 del Documento Maestro. Ver qué IMP de la zona 10–16 ms da el menor periodo real.
  - **Predicción:** escribirla aquí ANTES de correr el v5.
- **Orden 0b vs webapp:** se llevan en paralelo; el 0b decide si el hub puede servir la página.

## Hecho

| Fecha | Qué | Evidencia |
|---|---|---|
| 20-sep-2026 | Experimento 0 aprobado: σ = 168 µs | `experimentos/exp0/experimento0_resultados.md` |
| 11-oct-2026 | Smoke test y barridos v3/v4 del VL53L1X | Documento Maestro 9.2 (falta reporte propio) |
| 23-oct-2026 | Repo reorganizado, CLAUDE.md alineado, v5 corregido (no compilaba) | commits `a1c3931`, `b705d4e` y el de la corrección del v5 |
