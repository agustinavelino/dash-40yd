# ADR-014 — Carga compartida: el USB alimenta al sistema mientras carga la celda

- **Estado:** aceptado (12-nov-2026, decisión del usuario)
- **Modifica:** ADR-013, en un punto: "Sus 5 V van solo al cargador". Todo lo demás del ADR-013 sigue igual.
- **Origen:** auditoría de la guía del esquemático v3, hallazgo H4 (`hardware/auditoria_guia_v3.md`).

## Contexto

Con el ADR-013, los 5 V del USB-C solo van al TP4056, y el sistema se alimenta siempre de la celda a través del buck-boost. Eso trae tres problemas:

1. **La carga no termina en el uso normal de banco.** El TP4056 no separa la carga de la celda del consumo del sistema: corta cuando la corriente de su salida baja a C/10 (58 mA con R_PROG de 2 kΩ). Con el sistema encendido, la salida nunca baja de ~80–120 mA, así que la celda se queda a 4.2 V durante horas. El TP4056 no tiene temporizador que lo corte. El ADR-013 lo aceptaba como limitación porque "no se carga en campo", pero para programar el interruptor tiene que estar encendido: **pasa en cada sesión de desarrollo**.
2. **Sin celda, o con la celda agotada, no se puede programar.**
3. **El ESP32 solo recibe la energía que deja pasar la celda**, aunque haya un USB conectado.

## Decisión

Agregar un circuito de **carga compartida** con tres piezas, todas de la categoría Basic de JLCPCB:

```
VBUS ──|>|── D6 (Schottky B5819W) ────┐
                                      ├── VPATH ── interruptor (J2) ── VSYS ── R4 ── VBB ── TPS63021 ── +3V3
BATP ── Q7 (AO3401A: drain a BATP) ───┘
         gate a VBUS · R28 10 kΩ de gate a GND
```

- **Sin USB:** R28 jala el gate a GND, Q7 conduce y la celda alimenta VPATH (igual que hoy, más una caída de ~60 mΩ).
- **Con USB:** el gate queda a 5 V y Q7 se apaga. VPATH queda a ~4.8 V por D6, por encima de la celda, así que el diodo interno de Q7 queda en inversa. El sistema se alimenta del USB y la celda solo se carga: **la carga termina bien**.
- **El riel de 3.3 V sigue teniendo una sola fuente**: el TPS63021. El USB alimenta su *entrada*, nunca el riel. El ESP32 nunca recibe 5 V.
- **El interruptor sigue mandando en los dos casos**, porque la carga compartida va antes de él. Con el interruptor apagado, el USB solo carga.

## Alternativas consideradas

| Alternativa | Por qué no |
|---|---|
| Dejarlo como está (ADR-013) | Los tres problemas de arriba, en cada sesión de banco |
| Cargador con gestión de potencia integrada (familias BQ2407x, MCP73871) | Otro integrado Extended, otro circuito típico que revisar y más costo, para el mismo resultado |
| Jumper manual para elegir la fuente | Depende de acordarse; un error deja dos fuentes o ninguna |

## Comprobaciones

- **Entrada del TPS63021:** máximo recomendado 5.5 V (SLVS916I, condiciones recomendadas). VBUS es como máximo ~5.25–5.5 V y D6 le quita ~0.15–0.4 V, así que queda dentro, con poco margen en el peor caso. Máximo absoluto: 7 V.
- **Gate de Q7:** el AO3401A tolera ±12 V entre gate y source; VBUS llega a 5.5 V como máximo.
- **El ESP32 sigue alimentado al desconectar el USB:** el diodo interno de Q7 conduce en cuanto VPATH baja de la celda, y el bulk sostiene el riel mientras el gate cae. **No medido**; se verifica en la primera energización.

## Consecuencias

- **Programar ya no requiere celda**, pero sí el interruptor encendido: el CH340C se alimenta de +3V3, como en el ADR-013.
- **Corriente del USB:** ahora el USB entrega carga + sistema (~580 + ~120 mA con R_PROG de 2 kΩ). **Propuesta:** R_PROG = 3 kΩ (~400 mA, tabla del TP4056) para quedar cerca de los 500 mA de un puerto USB 2.0. Una celda de 3 000 mAh carga en ~8 h, de noche. Decisión del usuario, junto con H2.
- **Lectura de batería:** el divisor de GPIO34 mide VSYS. Con USB conectado lee ~4.8 V, no la celda. El firmware puede interpretar **VSYS > 4.4 V como "USB conectado / cargando"**; así no hace falta otra señal.
- **Fuga con el equipo apagado:** sin USB, la celda queda conectada a VBUS a través de la fuga en inversa de D6, y de ahí a GND por R28. Se suma a la del TVS (auditoría, H6). Se mide en la primera placa. R28 es de 10 kΩ y no de 100 kΩ para que esa fuga no suba el voltaje del gate al punto de apagar Q7 en caliente.
- **Hipótesis para E1, no medida:** el TPS63021 funciona desde 1.8 V de entrada, así que una caída del USB de la laptop casi no lo afecta. Las DevKit regulan con un LDO que necesita ~4.3 V o más; eso podría explicar parte de los brownouts del Exp. 0b alimentado por USB.
- **Documento Maestro (v2.4):** 12.1 y 12.2 ("los 5 V solo van al cargador") y 18 (agregar este ADR). La regla de carga de 11.2 se revisa aparte (auditoría, sección 4).
- **Guía del esquemático:** aplicado en la v4 (12-nov-2026).
