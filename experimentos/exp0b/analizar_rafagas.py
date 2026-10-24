#!/usr/bin/env python3
"""
Analisis de la prueba de rafagas del Experimento 0b - DASH Timing System

El 0b con 200 pulsos solo tiene 4 rafagas de resync por corrida, y la sigma
de cada corrida la decide casi entera la peor de ellas. Esta prueba repite
la MISMA rafaga (hacerResync() sin cambios) ~100 veces por condicion para
medir directamente si la carga del SoftAP hace mas frecuentes las rafagas
malas (las 7 pings lentos).

Lee las lineas "B,rafaga,mejor_rtt_us,pongs,offset_us,paginas,clientes_ws*HH"
que imprime prueba_0b_hub compilado con -DMODO_RAFAGAS=1.

Metricas por condicion:
  - mejor rtt por rafaga: mediana, p90, maximo, % por encima de 3 ms
  - cota de error por rafaga = (mejor_rtt - F) / 2, con F = 2332 us (Exp. 0)
  - error real del offset: residuo de cada offset respecto a la recta de
    deriva ajustada a toda la corrida (la deriva es lineal en minutos,
    Exp. 0 seccion 6.2). Es el numero que entra al resultado.

Uso:  python3 experimentos/exp0b/analizar_rafagas.py <archivo> [<archivo> ...]
"""
import os, statistics as st, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
from resumen_corrida import checksum_ok

F_US = 2332            # piso fisico de round-trip medido en el Exp. 0
PERIODO_S = 3.0        # una rafaga cada 3 s (PERIODO_RAFAGA_MS del sketch)


def leer(ruta):
    """Primera copia valida de cada rafaga. Exige checksum."""
    rafagas = {}
    with open(ruta, encoding="latin-1") as f:
        for linea in f:
            s = linea.strip()
            if not s.startswith("B,") or "*" not in s:
                continue
            datos, ok = checksum_ok(s)
            c = datos.split(",")
            if not ok or len(c) != 7:
                continue
            n, rtt, pongs, off, pag, cli = (int(x) for x in c[1:])
            rafagas.setdefault(n, dict(rtt=rtt, pongs=pongs, off=off, pag=pag, cli=cli))
    return [dict(n=k, **v) for k, v in sorted(rafagas.items())]


def pct(v, q):
    v = sorted(v)
    return v[min(len(v) - 1, int(q * len(v)))]


def analizar(ruta):
    r = [x for x in leer(ruta) if x["rtt"] >= 0]      # rtt -1 = no llego ningun pong
    if len(r) < 3:
        print(f"{os.path.basename(ruta)}: solo {len(r)} rafagas validas")
        return
    rtt = [x["rtt"] for x in r]
    # recta de deriva: offset contra tiempo
    t = [x["n"] * PERIODO_S for x in r]
    o = [x["off"] for x in r]
    mt, mo = st.mean(t), st.mean(o)
    m = sum((a - mt) * (b - mo) for a, b in zip(t, o)) / sum((a - mt) ** 2 for a in t)
    resid = [b - (mo + m * (a - mt)) for a, b in zip(t, o)]
    malas = sum(x > 3000 for x in rtt)
    dp = r[-1]["pag"] - r[0]["pag"]
    dur = (r[-1]["n"] - r[0]["n"]) * PERIODO_S
    print(f"{os.path.basename(ruta)}")
    print(f"  rafagas validas {len(r)}  pongs promedio {st.mean(x['pongs'] for x in r):.2f}/7"
          f"  clientes_ws mediana {st.median(x['cli'] for x in r):.0f}"
          f"  paginas {dp} en {dur:.0f} s = {dp / dur:.2f}/s")
    print(f"  mejor rtt   mediana {st.median(rtt):.0f}  p90 {pct(rtt, .9)}  max {max(rtt)} us"
          f"   rafagas > 3 ms: {malas}/{len(r)} ({100 * malas / len(r):.0f} %)")
    print(f"  cota error  mediana ±{(st.median(rtt) - F_US) / 2:.0f}  max ±{(max(rtt) - F_US) / 2:.0f} us")
    print(f"  error real del offset (residuo a la recta de deriva {m:.1f} us/s):"
          f"  sigma {st.pstdev(resid):.0f}  max |{max(abs(x) for x in resid):.0f}| us")


for ruta in sys.argv[1:]:
    analizar(ruta)
