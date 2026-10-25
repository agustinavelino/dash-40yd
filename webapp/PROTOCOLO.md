# Protocolo hub ↔ página

Contrato entre el hub (nodo de 40 yd) y la página del coach. El firmware de producto lo implementa del lado del ESP32. `index.html` y `sim.js` lo implementan del lado del navegador. La futura capa de nube consume los mismos formatos (`/api/sesion` y el CSV), así que **cambiar algo aquí es cambiar tres cosas**: se anota en la tabla de versiones del final.

Referencias del Documento Maestro: 13.3 (página local), 7.2 (flujo de una corrida), REQ-F-04 a 07, REQ-NF-08, ADR-010 (distancias configurables), 6.1 (centésimas).

## Unidades y convenciones

- **Tiempos en milisegundos enteros** en todo lo que viaja por la red. El hub mide en µs. Pasar a ms enteros pierde 0.5 ms como máximo, contra un error del sistema de decenas de ms (sección 6). La página muestra centésimas de segundo.
- **Splits medidos desde T0**, el levantamiento de mano en la puerta de 0 yd (ADR-004): `[t1, t2, t3]` = tiempo acumulado al cruzar la puerta 1, 2 y 3.
- **Puerta sin detección:** su split es `null`. Nunca se rellena ni se interpola.
- **Distancias de puerta en yardas**, configurables (ADR-010). La puerta 0 siempre está en 0. Cada corrida guarda las distancias vigentes cuando se midió, para que mover una puerta después no reescriba el pasado (REQ-F-06).
- **Ids:** `atleta` es un entero asignado por el hub; `corrida` es un entero creciente dentro de la sesión.
- **Hora:** el ESP32 no tiene reloj de tiempo real. Al conectar, la página manda la hora del teléfono (`c:"hora"`). Si nadie la ha mandado, `ts` es `null`.

## WebSocket `ws://192.168.4.1/ws`

Un mensaje = un objeto JSON en un frame de texto. Los del hub llevan `t` (tipo) y los de la página llevan `c` (comando).

### Del hub a la página

**`estado`.** Se envía al conectar, cada vez que algo cambia y como latido cada 5 s. Si pasan más de 12 s sin mensajes, la página da la conexión por caída y reconecta.

```json
{"t":"estado","armado":false,"atleta":3,
 "puertas":[{"i":0,"yd":0,"ok":true,"resync_s":4},
            {"i":1,"yd":10,"ok":true,"resync_s":4},
            {"i":2,"yd":20,"ok":false,"resync_s":null},
            {"i":3,"yd":40,"ok":true,"resync_s":0}]}
```

- `ok`: el hub ha oído a esa puerta en los últimos 35 s (un ciclo de resync más margen).
- `resync_s`: segundos desde su último resync válido; `null` si no hay. La puerta 3 es el propio hub (siempre 0).

**`corrida`.** Se envía cuando el hub cierra una corrida: llegaron todas las puertas, o venció la ventana de espera tras la meta.

```json
{"t":"corrida","id":12,"atleta":3,"splits_ms":[1742,2915,5108],"yd":[10,20,40],"ts":1761350400}
```

**`error`.** Para un comando rechazado.

```json
{"t":"error","msg":"no se puede armar sin atleta"}
```

### De la página al hub

| Comando | Ejemplo | Efecto |
|---|---|---|
| `hora` | `{"c":"hora","epoch":1761350400}` | Fija el reloj del hub (lo manda la página al conectar) |
| `armar` | `{"c":"armar","atleta":3}` | Selecciona atleta y arma; el hub responde con `estado` |
| `desarmar` | `{"c":"desarmar"}` | Desarma sin registrar corrida |

Después de una `corrida`, el hub **queda desarmado**. Cada corrida se arma a propósito, así una persona caminando por el carril no genera corridas falsas.

## HTTP

| Método y ruta | Cuerpo / respuesta |
|---|---|
| `GET /` | La página (gzip) |
| `GET /api/roster` | `[{"id":3,"nombre":"Diego R.","num":22}, …]` |
| `POST /api/roster` | La lista completa; el hub la guarda en flash y asigna `id` a las entradas que llegan con `id` `null`. Responde la lista guardada |
| `GET /api/config` | `{"yd":[10,20,40]}` |
| `POST /api/config` | `{"yd":[10,20,40]}`. Distancias crecientes y > 0; si no, 400 |
| `GET /api/sesion` | `{"inicio":epoch\|null,"corridas":[<objetos como en el mensaje corrida, sin "t">]}` |
| `GET /sesion.csv` | El CSV de abajo, generado por el hub |

La página puede armar el CSV ella misma a partir de `/api/sesion`, con el mismo formato. Así funciona igual con el simulador.

## CSV de sesión

Separador `,`, decimal `.`, UTF-8, una fila por corrida, en orden de `id`:

```
corrida,atleta,num,t1_s,t2_s,t3_s,yd1,yd2,yd3,fecha
12,Diego R.,22,1.74,2.92,5.11,10,20,40,2026-10-24 17:20
13,Diego R.,22,1.70,,5.02,10,20,40,2026-10-24 17:24
```

- Tiempos en segundos con **dos decimales** (sección 6.1: el sistema no distingue 5.10 de 5.12).
- Un split vacío es una puerta sin detección.
- `fecha` en hora local del teléfono que mandó `hora`; vacía si `ts` es `null`.
- Si el nombre contiene una coma, va entre comillas dobles.

## Versiones

| Versión | Fecha | Cambio |
|---|---|---|
| 1 | 24-oct-2026 | Primera versión (W1, prototipo con simulador) |
