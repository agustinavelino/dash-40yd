# ADR-013 — Módulo ESP32 soldado y placa ensamblada por el fabricante

- **Estado:** aceptado (24-oct-2026, decisión del usuario)
- **Reemplaza:** Documento Maestro v2.2, sección 12.1 ("headers hembra para socketear la tarjeta ESP32, nunca soldar directo") y 12.5 ("aunque el ensamble sea manual")
- **Numeración:** ADR-012 queda reservado para el reintento de ráfagas, que el Maestro v2.2 (sección 18) ya nombra así como candidato.

## Contexto

El Maestro v2.2 define la placa como **portadora**: la tarjeta de desarrollo ESP32 se enchufa en headers hembra, y el cargador y el buck-boost son módulos genéricos. Al escribir la guía del esquemático (`hardware/guia_esquematico/`) aparecieron dos problemas de ese enfoque:

1. **Dos fuentes en el mismo riel.** El USB de la tarjeta genera 3.3 V con el regulador de la propia tarjeta, en el mismo pin que alimenta el buck-boost. El Maestro (12.2) pide tratar el USB "solo como datos", y con una tarjeta enchufada eso exige un jumper, un power mux o una regla de uso.
2. **Footprints inciertos.** Los módulos genéricos tienen dimensiones publicadas poco confiables (12.2). Hay que medirlos con calibrador antes de cerrar la placa, y pueden cambiar de un lote a otro.

Además, los headers se aflojan con la vibración y los golpes de un tripié en campo, y la tarjeta enchufada hace la placa y el gabinete más grandes.

## Decisión

- El **módulo ESP32-WROOM-32UE** (chip, flash, cristal y conector U.FL de antena, ya certificado) va **soldado** en la PCB. No se usa el chip ESP32 suelto: diseñar la parte de radiofrecuencia queda fuera del alcance.
- El cargador y el regulador se diseñan **con sus integrados**, no con módulos: TP4056 + DW01A + FS8205A para carga y protección, y TPS63021 (familia TPS63020, salida fija de 3.3 V) para el buck-boost.
- **Un solo USB-C** para cargar y programar. Sus 5 V van solo al cargador. El ESP32 se alimenta siempre de la batería a través del buck-boost, así que no hay dos fuentes en el riel de 3.3 V. Un convertidor USB-serial (CH340C) con auto-reset permite programar sin botones.
- **Ensamblado SMD por el fabricante** (JLCPCB o similar), con piezas de su inventario (LCSC). Todo lo SMD va en una sola cara.
- Se conserva todo lo demás de la sección 12: protección (PTC, polaridad inversa, TVS), conector de sensor de 6 vías con load switch y XSHUT, LED, buzzer, jumpers de rol, test points, 2 capas, 5 placas.

## Alternativas consideradas

| Alternativa | Por qué no |
|---|---|
| Tarjeta en headers (Maestro v2.2) | Se resuelve con jumper de fuente, pero sigue la fragilidad mecánica y los footprints de módulos genéricos |
| Módulo soldado a mano, sin ensamble | Soldar el WROOM-32UE y el TPS63021 (QFN) a mano es riesgoso y poco repetible en 5 placas |
| Chip ESP32 suelto | Exige diseñar el acoplamiento de RF y la flash; no aporta nada al proyecto |

## Consecuencias

- **Más circuito que diseñar y revisar:** USB-serial, auto-reset, arranque (EN e IO0) y el buck-boost según su hoja de datos. Se mitiga copiando el diseño de referencia de Espressif (ESP32-DevKitC) y el circuito típico de cada hoja de datos.
- **Si algo sale mal, ya no se arregla cambiando una tarjeta.** Sube el riesgo "PCB en ruta crítica sin holgura para un respin" (Maestro 17), que ya era alto. Mitigaciones:
  - test points en cada riel;
  - jumpers de 0 Ω para aislar bloques en la primera energización;
  - revisión del esquemático contra la guía antes de rutear.
- **Costo:** sube respecto a los $600 MXN de fabricación del Maestro 15.2, por el ensamblado y las piezas "extendidas". **Pendiente:** cotización real de JLCPCB para 5 placas.
- **Programar y cargar** van por el mismo USB-C. Para programar, el interruptor debe estar encendido: el CH340C se alimenta del riel de 3.3 V.
- **Limitación conocida:** el TP4056 no administra la energía mientras carga. Con el sistema encendido durante la carga, la terminación de carga no es confiable. Coincide con la regla operativa del Maestro (no cargar en campo) y se documenta en la serigrafía.
- **Experimentos:** las tarjetas de desarrollo siguen siendo el banco de todos los experimentos; este ADR solo cambia la placa de producto.
- **Documento Maestro:** actualizar 12.1, 12.2 (quitar "medir módulos genéricos" para el cargador y el buck-boost), 12.5, 15.1 y 15.2 en la v2.3.
