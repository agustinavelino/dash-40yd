#!/usr/bin/env python3
"""
Fase de las interrupciones del VL53L1X alrededor de los cruces - DASH Timing System

Complementa a analizar_cielo.py. Ese script cuenta intervalos anomalos; este
mide la FASE de cada interrupcion para saber que significan.

Fase de la muestra i = t_i - mediana(t_j + (i - j) * periodo_medio) sobre sus
vecinas j. Una muestra con fase ~0 llega a tiempo respecto a sus vecinas; una
con +9.9 ms llega tarde.

Imprime:
  1. Muestras tardias (> +4.5 ms) y tempranas (< -4.5 ms) respecto a +-20 vecinas,
     con el status de la muestra. Un par de intervalos "+ -" seguidos es UNA
     interrupcion tardia, no dos anomalos independientes.
  2. Por cada cruce (primera muestra valida tras una invalida): su fase respecto
     a las 10 muestras ANTERIORES (sin blanco) y a las 10 POSTERIORES (con blanco).

Uso:  python3 experimentos/sensor_vl53l1x/fase_cruces.py <captura.txt> [...]
"""
import collections, os, statistics as stat, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from analizar_cielo import leer

UMBRAL = 4500   # us: media ranura de anomalo (~9.5 ms) para separar "a tiempo" de "fuera de fase"


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    for ruta in sys.argv[1:]:
        _, m, _, _ = leer(ruta)
        n = len(m)
        t = [x[0] for x in m]
        per = (t[-1] - t[0]) / (n - 1)

        def fase(i, js):
            return stat.median(t[i] - t[j] - (i - j) * per for j in js)

        res = [fase(i, range(max(0, i - 20), min(n, i + 21))) for i in range(n)]
        tarde = [i for i in range(n) if res[i] > UMBRAL]
        temprana = [i for i in range(n) if res[i] < -UMBRAL]
        print(f"archivo   {ruta}")
        print(f"          {n} muestras, periodo medio {per / 1000:.3f} ms")
        st_tarde = collections.Counter(m[i][1] for i in tarde)
        print(f"tardias   {len(tarde)} (> +4.5 ms), retraso mediana {stat.median(res[i] for i in tarde) / 1000:.1f} ms"
              f" | status: " + "  ".join(f"{k}:{v}" for k, v in sorted(st_tarde.items())))
        print(f"tempranas {len(temprana)} (< -4.5 ms)")

        clases = collections.Counter()
        antes_ms, despues_ms = [], []
        for i in range(10, n - 10):
            if m[i][1] == 0 and m[i - 1][1] != 0:
                a = fase(i, range(i - 10, i - 1))
                d = fase(i, range(i + 2, i + 11))
                clases[("tarde" if a > UMBRAL else "a tiempo", "tarde" if d > UMBRAL else "a tiempo")] += 1
                antes_ms.append(a / 1000)
                despues_ms.append(d / 1000)
        print(f"cruces    {sum(clases.values())}: fase de la 1a valida contra las 10 anteriores"
              f" mediana {stat.median(antes_ms):+.1f} ms, contra las 10 posteriores {stat.median(despues_ms):+.1f} ms")
        for (a, d), k in sorted(clases.items()):
            print(f"          {k:>3} cruces: {a} respecto a sin blanco, {d} respecto a con blanco")
        print()


if __name__ == "__main__":
    main()
