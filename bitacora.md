# Prueba 0 — Jitter de la infraestructura de tiempo

## El problema: no hay un reloj central

Como no se tiene un nodo central que tenga un reloj fijo compartido entre los cuatro puntos de medición del dash, usamos cuatro ESP32, uno por estación. Cada uno tiene su propio reloj interno, y ese reloj no "sabe la hora": solo cuenta microsegundos desde el instante en que el dispositivo se enciende.

Ejemplo: si el nodo A se enciende y empieza a contar, y cuatro segundos después se enciende el nodo B (que arranca su contador en cero), entonces dos segundos más tarde el nodo A marcará 6 segundos mientras que B apenas marcará 2 segundos. Ninguno de los dos está "mal" — simplemente arrancaron en momentos distintos. Esa diferencia constante entre ambos relojes se llama **offset**.

## Por qué el offset no se queda fijo: la deriva de cristal

El offset no se mantiene constante en el tiempo, porque ningún cristal de cuarzo oscila exactamente a la misma frecuencia que otro. Fabricarlos perfectamente idénticos es físicamente imposible: al cortar la lámina de cuarzo siempre queda una variación microscópica de grosor entre una pieza y otra, y eso hace que cada cristal resuene a una frecuencia ligeramente distinta de la nominal (aunque todos digan "40 MHz" en la hoja de datos).

Esa variación se mide en **PPM — partes por millón** (no "pulsos por minuto"; es una unidad de proporción, como decir "40 de cada millón"). Para el ESP32, la tolerancia individual de cada cristal es de aproximadamente **±40 ppm**, y entre dos cristales cualquiera la diferencia relativa puede llegar a **~35 ppm** — es decir, unos 35 microsegundos de diferencia por cada segundo transcurrido. Ese pequeño desfase, acumulado minuto a minuto, es lo que se llama **deriva**, y es la razón por la que el offset hay que recalcularlo periódicamente en vez de medirlo una sola vez.

Este offset lo calcularemos mediante un intercambio de mensajes entre los nodos usando el protocolo ESP-NOW.

## Dónde vive el cristal (aclaración de hardware)

El cristal de cuarzo no está "dentro de una placa metálica" en el sentido de una placa completa — esa pieza metálica que se ve en el ESP32 es la tapa de blindaje de un **módulo**: una mini-placa propia que trae soldados el chip ESP32, el cristal de cuarzo y, en algunas versiones, también la memoria flash, todo cubierto por esa tapa. La tapa existe para blindaje electromagnético: evita que el ruido de otros componentes de la placa contamine la señal de radio del WiFi o Bluetooth.

## Por qué la calibración no se "guarda" — se recalcula en vivo

Como cada ESP32 trae su propio cristal de cuarzo con un ppm ligeramente distinto, **no existe un solo número de offset que se pueda calcular una vez y reutilizar después** — ese número es específico del par de cristales físicos que estén conectados en ese momento. Si mañana cambiamos un nodo por otro ESP32 (por ejemplo, al pasar del DevKitC a los módulos SMD que aún no tenemos), el offset entre ese nuevo par será distinto, aunque sea ligeramente.

Lo que sí es reutilizable —y es lo que realmente estamos construyendo— es el **protocolo**: el algoritmo de firmware que mide el offset actual en vivo, cada 30 segundos, sin importar qué par físico de módulos esté insertado. No se calibra una vez; se construye un sistema que se recalibra solo, todo el tiempo, de forma automática.

## Arquitectura: timestamp distribuido

Para esta prueba se valida la arquitectura en la que cada nodo marca su propio timestamp localmente y luego se resincroniza con los demás. Bajo este esquema ya no importa cuánto tarde en llegar el paquete de datos por radio — el instante del evento quedó "congelado" localmente en el momento en que ocurrió, no en el momento en que se recibe. El objetivo de la prueba es verificar que esta arquitectura efectivamente aporta solo entre 1 y 2 milisegundos de error, tal como predice el diseño.

## La referencia: un pulso por cable

Para medir el error de un sistema se necesita una referencia más exacta que el propio sistema que se está midiendo. Por eso usaremos un cable físico entre los dos nodos para generar un evento eléctrico compartido: la electricidad viaja por el cable en nanosegundos, seis órdenes de magnitud más rápido que el error que buscamos medir (milisegundos). Eso significa que, para efectos prácticos, el instante real en que ambos nodos "ven" el pulso es el mismo — y por lo tanto, cualquier diferencia que midamos entre ambos timestamps es, por descarte, puro error de nuestra infraestructura de sincronización inalámbrica.

---

## Por qué el resync es cada 30 segundos (y no más seguido)

Ya vimos que existe un offset entre los relojes de A y B, y que ese offset **deriva** con el tiempo porque cada cristal de cuarzo es distinto. La solución es recalcular el offset periódicamente — a este recálculo periódico le llamamos **resync**.

Calcular el offset en sí no es una tarea complicada; lo importante es hacerlo con la frecuencia correcta para que la deriva acumulada entre un resync y el siguiente no crezca demasiado. El objetivo de resincronizar cada 30 segundos es que el error acumulado por deriva se quede en, más o menos, 1 ms.

Podría resincronizarse cada 5 segundos, por ejemplo, para reducir aún más ese error — pero cada resync consume tráfico de radio y batería, y el sensor de distancia elegido ya introduce por sí solo un margen de error mucho mayor (geometría corporal, ±20–50 ms). Reducir la deriva de 1 ms a 175 µs no movería la aguja en nada perceptible; solo gastaría recursos donde ya no hace falta.

> **Regla práctica:** el resync solo necesita ser lo bastante frecuente para mantener la deriva muy por debajo del error dominante del sistema. Ir más rápido no mejora el resultado real — solo consume batería y ancho de banda de radio sin necesidad.

## Cómo se calcula el offset — la idea

Para calcular el offset hay que fijarse en dos momentos: cuándo se envía el paquete, y cuándo llega la respuesta. Como las condiciones del viaje de ida y de vuelta son similares (misma distancia, mismas condiciones de radio en ese instante), se asume que el tiempo de ida es parecido al de vuelta.

Con esa suposición, se puede estimar *a qué hora, según el propio reloj de A, debió haber llegado el paquete a B* — tomando el punto medio entre el momento en que A lo mandó y el momento en que A recibió la respuesta, y promediándolos. Bajo esa suposición de simetría, la latencia de ida y la de vuelta se cancelan entre sí, y lo que queda es justo esa estimación del punto medio.

Ese punto medio se compara contra el timestamp que realmente reportó el nodo B en ese instante. La diferencia entre ambos **es el offset** — y de paso, comparar contra el dato real de B es lo que permite comprobar empíricamente si la suposición de simetría fue acertada o no (si no lo fue, ese error se refleja en el resultado).

## La fórmula

$$
\text{offset} = t_2 - \frac{t_1 + t_4}{2}
$$

| Variable | Qué es |
|---|---|
| `t1` | Hora local de A cuando envía el paquete |
| `t4` | Hora local de A cuando recibe la respuesta |
| `t2` | Hora local de B cuando recibe el paquete de A |
| `(t1+t4)/2` | Estimación de A de "a qué hora, en mi propio reloj, debió llegar el paquete a B" |
| `offset` | Diferencia entre lo que B decía y lo que A esperaba |