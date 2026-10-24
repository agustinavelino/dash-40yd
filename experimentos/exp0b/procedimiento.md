# Experimento 0b — procedimiento de corrida

Plan completo, criterios y trampas: `docs/experimento0b_contexto.md`. Aquí está solo **cómo se corre**.

## Antes de empezar

1. **Predicción escrita** en `docs/ESTADO.md` (decisiones abiertas → Exp. 0b). Sin ella no se corre.
2. Montaje del Exp. 0 sin cambios: GPIO4 del hub → 330 Ω → GPIO16 del hub y GPIO16 del nodo, **tierra común**, sensor desconectado, las dos placas por USB.
3. Teléfonos (para S2 y S3):
   - **Datos móviles apagados.** Si no, Android/iOS ve que "DASH-0b no tiene internet" y se pasa a datos sin avisar.
   - Conectar una vez a la red **DASH-0b** (contraseña `dash0b2026`) y marcar "conectar automáticamente" / "mantener conexión aunque no tenga internet".
   - Pantalla siempre encendida mientras dura la corrida (desactivar el bloqueo automático).
   - Página: `http://192.168.4.1/` (escucha) o `http://192.168.4.1/?estres=1` (recarga cada 1 s).
4. Todas las corridas el **mismo día y con el mismo montaje**. C0 se repite ese día aunque ya exista el Exp. 0 (trampa 4).

## Binarios

| Escenario | Hub | Nodo |
|---|---|---|
| C0 | `prueba_0b_hub` con `ARD_FLAGS=-DSOFTAP_ACTIVO=0` | `prueba_0b_nodo` |
| S1, S2, S3 | `prueba_0b_hub` (SoftAP activo por defecto) | `prueba_0b_nodo` |

El hub siempre con `ARD_OPCIONES=PartitionScheme=huge_app`: la página de 237 KB no cabe en la partición por defecto.

Si se cambia la plantilla de la página: `python3 experimentos/exp0b/generar_pagina.py` antes de compilar.

## Escenarios

| ID | Qué hace el usuario | Clientes esperados (`clientes_ws`) |
|---|---|---|
| C0 | Nada. SoftAP apagado; el hub arranca sin espera | 0 |
| S1 | Nada. SoftAP encendido, ningún teléfono conectado. El hub espera 60 s y arranca solo | 0 |
| S2 | 1 teléfono en `http://192.168.4.1/` (escucha). Cuando diga "WebSocket conectado", tocar **Iniciar corrida** | 1 |
| S3 | 1 teléfono en `/?estres=1` + 1–2 teléfonos en `/` (escucha). Cuando los de escucha estén conectados, tocar **Iniciar** en uno de ellos | ≥ 2 (fluctúa por las recargas) |

En S2 y S3, abrir el puerto serial reinicia el hub y el SoftAP se cae unos segundos: los teléfonos se reconectan solos y la página reintenta el WebSocket cada 1 s. El hub espera hasta 60 s o hasta que alguien toque **Iniciar**.

## Orden en cada escenario (lo ejecuta el runner)

1. Identificar puertos por MAC.
2. Flashear **hub → nodo → logger en el hub**, sin pausas (`runner.md`, paso 5b).
3. Logger: `--fin "^# fin$" --max-s 450 --idle-s 300`. **Exacto con `$`**: `^# fin` también atrapa la línea `# fin de espera` y corta la captura a los 60 s (pasó en S1). La impresión es diferida: durante la corrida no sale nada.
4. Archivo: `experimentos/exp0b/exp0b_<ID>_<YYYY-MM-DD>.csv`.
5. Resumen: `python3 tools/resumen_corrida.py <archivo> --esperados 200`.

El nodo no se reflashea entre escenarios si ya tiene `prueba_0b_nodo`, pero **sí se reinicia**: el handshake solo se acepta una vez por arranque. El runner lo reflashea para garantizarlo.

## Verificaciones por corrida

- Encabezado: `# exp0b hub | SOFTAP_ACTIVO=… | canal 1 | …`. Canal ≠ 1 → trampa 1.
- `# fin de espera (boton|tiempo) | estaciones_ap N | clientes_ws M` coincide con el escenario.
- 200/200 eventos; `clientes_ws` dentro de lo esperado.
- Líneas `R,…` presentes (round-trips de todas las ráfagas).
- Cada línea de datos termina en `*HH` (checksum) y la tabla sale dos veces; `resumen_corrida.py` descarta las corruptas y usa la primera copia válida. Si reporta "lineas corruptas descartadas", es pérdida del serial, no del radio.

## Criterios de éxito (plan, 3.3)

- σ(S3) ≤ ~1.5 × σ(C0 del mismo día) y 0 eventos perdidos.
- σ ≤ 5 ms en todos (REQ-NF-02).
- Señal temprana: mediana del rtt elegido muy por encima de ~2.5 ms, o pongs promedio < 7.
