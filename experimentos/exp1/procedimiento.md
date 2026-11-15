# Experimento 1 — Procedimiento de banco

Contribución del sensor al error (Documento Maestro v2.3, sección 16.3). Escrito el 15-nov-2026, **antes** de la primera corrida.

## Qué se mide

Dos VL53L1X ven el mismo cruce de una cartulina, uno conectado al hub y otro al nodo. El valor verdadero de la diferencia entre sus timestamps es **cero** por construcción. Lo que salga es error:

- `resultado = t_hub − (t_nodo − offset)` (la fórmula del Exp. 0)
- `σ_sensor = √((σ_exp1² − σ_exp0²) / 2)` con σ_exp0 = 168 µs

**Predicción escrita (Maestro 16.3, no se ajusta):** σ_exp1 = **9.5 ms** con ventanas uniformes, **10.7 ms** con la tasa de anómalos de la 9.2. El usuario no agrega predicción propia.

**Hipótesis de S2b (no es predicción):** si la ventana que termina en la detección es el intervalo largo de ~33.2 ms en cada cruce, σ_exp1 ≈ 13.6 ms. Con unas 200 pasadas el IC 95 % de σ es de ±8 %: alcanza para separar 9.5 de 13.6 ms, no 9.5 de 10.7.

**Interpretación de la 16.3 (anotar para la v2.4 del Maestro):** "uno conectado a cada nodo" implica un sensor por placa, cada uno en su propio bus I2C y con la dirección 0x29. El párrafo de XSHUT ("direcciones I2C distintas en el mismo bus") solo aplicaría con los dos sensores en una sola placa. Se sigue la primera lectura porque la resta de σ_exp0 la supone. XSHUT se cablea igual, para el reset por hardware (regla 9).

## Material

- Las dos placas (hub `A4:F0:0F:64:AC:68`, nodo `B0:CB:D8:CA:11:F0`) y dos cables USB a la laptop.
- Dos VL53L1X con los 6 pines accesibles (VIN, GND, SDA, SCL, GPIO1, XSHUT).
- 12 jumpers de ~20 cm, 6 por sensor.
- El LED externo con su resistencia de 330 Ω (el de S2b).
- Una tablita rígida y plana (MDF, acrílico o cartón grueso) de unos 10 × 10 cm y cinta doble cara.
- La cartulina: rígida, de al menos 30 × 40 cm, con el borde de entrada recto.
- Calibrador y escuadra.

## Cableado (las dos placas iguales)

| Pin del módulo VL53L1X | Pin del ESP32 | Nota |
|---|---|---|
| VIN | **3V3** | **No** al pin VIN/5V del ESP32 |
| GND | GND | |
| SDA | GPIO21 | |
| SCL | GPIO22 | |
| GPIO1 (INT) | GPIO16 | |
| XSHUT | GPIO17 | |

- **Solo en el hub:** GPIO18 → resistencia de 330 Ω → pata larga (ánodo) del LED; la pata corta → GND.
- **El cable del Exp. 0 (GPIO4 → 330 Ω → GPIO16) se quita de las dos placas.** GPIO16 ahora es el INT del sensor.
- No hay ningún cable entre las dos placas: solo se comunican por radio. Tampoco hace falta tierra común, porque no comparten ninguna señal eléctrica (en el Exp. 0 sí la compartían). Las dos van a la misma laptop por USB.

## Montaje

```
  VISTA DESDE ARRIBA (los dos sensores miran al cielo)

                        ──── la cartulina avanza ────►
   ┌──────────┐
   │cartulina │              ┌───────┐
   │ plana,   │              │  ◉ H  │  sensor del hub
   │ a ~50 cm │              │  ◉ N  │  sensor del nodo
   └──────────┘              └───────┘
        ║ borde de entrada:    tablita
          paralelo a la línea H–N

  VISTA DE LADO
        cartulina plana ───►
     ════════════                ← a 40–70 cm, siempre a la misma altura
          \  |  /   \  |  /
           \ | /     \ | /       haces de 27° (~24 cm de ancho a 50 cm)
            [H]       [N]        en la tablita, sobre la mesa
```

1. **Pega los dos módulos en la tablita, juntos y con las lentes hacia arriba.** La línea que une las dos lentes debe quedar **perpendicular** a la dirección en que pasa la cartulina. Así el borde de la cartulina entra a los dos haces al mismo tiempo. Si los pones uno detrás del otro en la dirección del movimiento, uno ve el cruce antes que el otro, y como la mano no va siempre a la misma velocidad, eso se vuelve error.
2. **Los dos tienen que apuntar exactamente hacia el mismo lado.** Si los dos están pegados planos sobre la misma tablita rígida, ya quedan paralelos. Revisa con la escuadra que ninguno quede ladeado por un cable o un pin doblado. 2° de diferencia separan los haces ~2 cm a 50 cm, unos 20 ms a la velocidad de la mano.
3. **Mide con el calibrador y anota:** (a) la separación entre los centros de las lentes en la dirección del movimiento (idealmente 0 mm) y (b) la separación en la otra dirección (lo que mide de ancho el módulo).
4. **Fondo sin retorno: el cielo abierto, en el mismo lugar que S2b.** Nada a menos de 1.5 m encima de los sensores: ni techo, ni ramas, ni tu cabeza. El cambio de fase de ~9.9 ms que queremos medir solo aparece sin blanco; contra un techo cercano no se vería.
5. Pon la tablita sobre una mesa y el LED donde lo veas sin inclinarte sobre los sensores. Los cables a la laptop deben quedar fuera de los haces.

## Paso 1: control de interferencia (unos 2 min)

Lo pide la 16.3 antes de la corrida. Sirve para ver si un sensor lee mal cuando el otro también está emitiendo.

1. Deja la cartulina **fija** y plana a ~50 cm encima de los dos sensores, cubriendo los dos haces (por ejemplo, sobre unas cajas o libros).
2. Se flashea `control_interferencia` en las dos placas (es el mismo binario; cada placa reconoce su rol por la MAC) y se captura el hub con `--fin "^# fin$" --max-s 120`.
3. Hace tres veces la secuencia: hub solo → los dos → nodo solo. Al final imprime una tabla y `OK` o `REVISAR` por sensor.
4. **Criterio (propuesto, es una preferencia):** con el otro sensor encendido, válidas ≥ 99 %, |Δ media| ≤ 10 mm y σ ≤ 2 × σ solo + 1 mm. Si sale `REVISAR`, inclina uno de los sensores unos grados hacia afuera y repite.

## Paso 2: la corrida (unos 13 min)

1. Se quita la cartulina fija. Los sensores ven el cielo.
2. **Orden de arranque (agente runner, regla 5b): hub → nodo → logger en el hub.** El logger usa `--fin "^# fin$" --max-s 1100 --idle-s 1000`, porque el hub no imprime nada durante los ~13 min de la corrida. Antes de lanzarlo, avisa que ya estás junto al montaje: la corrida empieza en cuanto se abre el puerto.
3. **Parpadeo rápido del LED (4 s): prepárate.**
4. Después vienen 220 ciclos de 3.5 s. **Cada vez que el LED se encienda (1 s), haz un cruce:**
   - Sostén la cartulina plana, a 40–70 cm sobre los sensores y siempre a la misma altura, empezando de un lado del montaje.
   - Pásala en un solo movimiento parejo, **siempre en el mismo sentido**, a un paso de mano normal (unos 30 cm en un tercio de segundo). Que el borde de entrada vaya recto y paralelo a la línea de las lentes.
   - Al terminar, regrésala **por fuera del haz**: súbela a más de 1.5 m o rodéala por un lado. No la regreses por encima de los sensores a baja altura, porque eso cuenta como otro cruce.
   - Si se te pasa un LED, espera al siguiente. No hagas dos cruces en el mismo ciclo.
5. Con el LED apagado, nada encima de los sensores, incluida tu mano o tu cabeza.
6. El resync se hace solo, con el LED apagado. No tienes que hacer nada.
7. Al terminar, el hub imprime todo (H = detecciones del hub, N = del nodo, R = resyncs) y `# fin`.

**Validez de la corrida (propuesto, escrito antes de correr):** al menos 180 pares; 0 detecciones durante el parpadeo previo; como máximo 5 ciclos con más de una detección del hub. Si no se cumple, el intento se guarda como `_intentoN_<motivo>` y se repite.

## Análisis

```
python3 experimentos/exp1/analizar_exp1.py experimentos/exp1/exp1_<fecha>.txt --csv experimentos/exp1/exp1_pares_<fecha>.csv
```

Entrega σ_exp1 con su IC 95 % por bootstrap, σ_sensor, la comparación contra 9.5 / 10.7 / 13.6 ms, la tabla del intervalo previo a la detección (normal ~23.3 ms o largo ~33.2 ms) por placa, y la σ de cada combinación. Las trazas (10 muestras antes de la detección y 5 después) quedan en el crudo para la interpolación del flanco (Maestro 9.5), que se calcula al documentar.

## Supuestos declarados

- **Detección por firmware:** una detección es la primera muestra con status 0 y distancia < 1000 mm después de al menos 5 muestras sin blanco. Su timestamp es el de la ISR de esa muestra. Se supone que da el mismo instante que el umbral por hardware del chip (9.4), porque en los dos casos el INT sale al terminar la medición. No está verificado.
- **Resync programado:** la ráfaga de resync (~200 ms) solo se lanza entre 2.3 y 3.1 s del ciclo, con el LED apagado. Así ninguna detección cae en el hueco en que el loop del hub no lee el sensor. El algoritmo no cambia, solo cuándo se llama; el periodo efectivo queda entre 30 y 33.5 s.
- Este experimento mide **varianza, no sesgo** (16.3). La corrección de media ventana (9.5) no queda validada.
