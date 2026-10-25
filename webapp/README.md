# webapp — página del coach

La sirve el hub (nodo de 40 yd) por su SoftAP. El coach la abre en `http://192.168.4.1/` desde el teléfono, sin internet. Requisitos: Documento Maestro 13.3, REQ-F-04 a 07 y REQ-NF-08.

| Archivo | Qué es | ¿Se embebe en el ESP32? |
|---|---|---|
| `index.html` | La página: En vivo, Sesión, Atletas y Ajustes. HTML, CSS y JS en un solo archivo, sin recursos externos | Sí (W2, con gzip) |
| `PROTOCOLO.md` | Contrato hub ↔ página (WebSocket, HTTP, CSV). El firmware lo implementa del lado del hub | No |
| `sim.js` | Hub simulado que implementa PROTOCOLO.md en el navegador | **No, nunca** |
| `probar.py` | Prueba automática del flujo completo con el simulador, más capturas | No |

## Abrirla sin placas

Abrir en el navegador `webapp/index.html?sim=1`. Funciona desde `file://` y sin red.

| Opción en la URL | Efecto |
|---|---|
| `&rapido=1` | La corrida llega 1 s después de armar (normalmente 3–6 s) |
| `&caida=20` | Corta la conexión cada 20 s durante 3 s, para ver la reconexión |

Desde la consola del navegador: `sim.corrida()`, `sim.puerta(2, false)` (puerta sin señal), `sim.caer()`.

El simulador guarda el roster y las distancias en `localStorage` (hace las veces del flash del hub). Para volver a los de fábrica: `localStorage.clear()`.

## Prueba automática

```
.venv/bin/pip install playwright && .venv/bin/python -m playwright install chromium   # una vez
.venv/bin/python webapp/probar.py            # capturas en webapp/capturas/ (ignorada por git)
```

Comprueba:
- tamaño (≤ 30 KB) y que no haya recursos externos;
- que no se pueda armar sin atleta;
- retardo de llegada a pantalla (REQ-F-05);
- centésimas, puerta sin detección, banda de conexión caída y reconexión;
- orden y filtro de la sesión;
- alta, edición y baja de atletas;
- validación de distancias y formato del CSV;
- que una corrida conserve la distancia con la que se midió (REQ-F-06).

Termina con `TODO OK` o con la lista de fallas.

## Decisiones de diseño

- **Tema claro fijo, alto contraste:** se usa a pleno sol. El amarillo es solo para "armado" y el rojo solo para fallos.
- **La franja de cancha:** cada puerta se dibuja a su distancia real y su split va encima de la línea de yarda. Si se mueve una puerta (ADR-010), la franja cambia.
- **Una puerta sin detección se muestra como "—"**, nunca se interpola.
- **Tiempos a centésimas:** el sistema no distingue 5.10 de 5.12 (Maestro 6.1).
- **Tras cada corrida el hub queda desarmado:** cada corrida se arma a propósito.
