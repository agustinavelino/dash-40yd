#!/usr/bin/env python3
"""
Analisis de la corrida larga apuntando al cielo - DASH Timing System

Lee la salida cruda de corrida_cielo.ino (lineas "M,k,t_us,status,dist_mm,marca*HH")
y separa los intervalos entre interrupciones por regimen:
  valido     las dos muestras con status 0 (RangeValid): hay blanco en el haz
  invalido   las dos sin retorno valido: cielo, la condicion normal de las puertas
  transicion una de cada una (entrada o salida de un cruce)

Cada intervalo se clasifica por cuantas ranuras dura (ranura = IMP x 0.931):
  1 ranura   |r - 1| <= 0.15     lo normal
  2 ranuras  |r - 2| <= 0.15     el margen se agoto: la medicion no cupo
  anomalo    cualquier otro      p. ej. el +9.3 ms (~+0.4 ranura) del smoke test a IMP 20

Se excluyen los intervalos con un extremo rescatado por la red de seguridad
(timestamp que no es de la ISR) o con una lectura perdida entre dos flancos.

Criterios (docs/ESTADO.md, tareas I5 y S2):
  - >= 1000 intervalos en regimen invalido
  - 0 intervalos de 2 ranuras en regimen valido

Uso:  python3 experimentos/sensor_vl53l1x/analizar_cielo.py <captura.txt>
"""
import collections, os, re, statistics as stat, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
from resumen_corrida import checksum_ok

TOL = 0.15


def leer(ruta):
    muestras, corruptas, imp = {}, 0, None
    with open(ruta, encoding="latin-1") as f:
        for linea in f:
            s = linea.strip()
            m = re.search(r"IMP (\d+) ms", s)
            if s.startswith("#") and m and imp is None:
                imp = int(m.group(1))
            if not s.startswith("M,"):
                continue
            datos, ok = checksum_ok(s)
            c = datos.split(",")
            if "*" not in s or not ok or len(c) not in (6, 7):
                corruptas += 1
                continue
            k, t, st, d, marca = (int(x) for x in c[1:6])
            cue = int(c[6]) if len(c) == 7 else -1       # -1: captura sin columna cue
            muestras.setdefault(k, (t, st, d, marca, cue))
    return imp, [muestras[k] for k in sorted(muestras)], sorted(muestras), corruptas


def clase(r):
    if abs(r - 1) <= TOL:
        return "1 ranura"
    if abs(r - 2) <= TOL:
        return "2 ranuras"
    return "anomalo"


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    imp, m, ks, corruptas = leer(sys.argv[1])
    if imp is None or len(m) < 3:
        sys.exit("SIN DATOS (no hay encabezado con IMP o faltan lineas M)")
    ranura = imp * 931            # us
    excl = collections.Counter()
    grupos = collections.defaultdict(list)            # regimen -> [(dt, clase)]
    for i in range(1, len(m)):
        (t0, s0, _, m0, _), (t1, s1, _, m1, _) = m[i - 1], m[i]
        if ks[i] != ks[i - 1] + 1:
            excl["hueco por linea corrupta"] += 1
            continue
        if m0 == 1 or m1 == 1:
            excl["rescate por nivel"] += 1
            continue
        if m1 == 2:
            excl["lectura perdida"] += 1
            continue
        reg = "valido" if s0 == 0 and s1 == 0 else "invalido" if s0 and s1 else "transicion"
        dt = t1 - t0
        grupos[reg].append((dt, clase(dt / ranura)))

    cruces = sum(1 for i in range(1, len(m)) if m[i - 1][1] != 0 and m[i][1] == 0)
    status = collections.Counter(x[1] for x in m)
    dur = (m[-1][0] - m[0][0]) / 1e6
    print(f"archivo   {sys.argv[1]}")
    print(f"IMP {imp} ms, ranura teorica {ranura / 1000:.2f} ms | {len(m)} muestras en {dur:.1f} s"
          f" | cruces (invalido -> valido) {cruces}"
          + (f" | lineas corruptas {corruptas}" if corruptas else ""))
    print("status    " + "  ".join(f"{k}:{v}" for k, v in sorted(status.items())) + "   (0 = RangeValid)")
    if excl:
        print("excluidos " + "  ".join(f"{k} {v}" for k, v in excl.items()))
    print(f"{'regimen':<11}{'n':>6}{'1 ranura':>10}{'2 ranuras':>11}{'anomalos':>10}{'% anom':>8}"
          f"{'mediana ms':>12}{'jitter us':>11}")
    for reg in ("valido", "invalido", "transicion"):
        g = grupos.get(reg, [])
        if not g:
            print(f"{reg:<11}{0:>6}")
            continue
        c = collections.Counter(x[1] for x in g)
        limpios = [dt for dt, k in g if k == "1 ranura"]
        jit = f"{stat.pstdev(limpios):.0f}" if len(limpios) > 1 else "-"
        print(f"{reg:<11}{len(g):>6}{c['1 ranura']:>10}{c['2 ranuras']:>11}{c['anomalo']:>10}"
              f"{100 * c['anomalo'] / len(g):>7.1f}%{stat.median(dt for dt, _ in g) / 1000:>12.3f}{jit:>11}")
        if c["anomalo"]:
            exceso = sorted(round((dt - ranura) / 1000, 1) for dt, k in g if k == "anomalo")
            print(f"{'':<11}exceso de los anomalos sobre 1 ranura (ms): mediana {stat.median(exceso)}"
                  f"  min {exceso[0]}  max {exceso[-1]}")
    if all(x[4] >= 0 for x in m):
        dentro = [x[1] for x in m if x[4] == 1]; fuera = [x[1] for x in m if x[4] == 0]
        vd = sum(s == 0 for s in dentro); vf = sum(s == 0 for s in fuera)
        print(f"cue       validas con LED encendido {vd}/{len(dentro)} ({100 * vd / max(1, len(dentro)):.0f} %)"
              f"   |   con LED apagado {vf}/{len(fuera)} ({100 * vf / max(1, len(fuera)):.1f} %)"
              "   (apagado deberia ser ~0 %: si no, el fondo da retorno y no es regimen invalido)")
        # El primer segundo tras apagarse el LED es la mano saliendo (reaccion humana).
        # Despues de eso, una muestra valida solo puede venir del fondo.
        fondo, n_fondo, apagado = 0, 0, None
        for i, x in enumerate(m):
            if x[4] == 0 and (i == 0 or m[i - 1][4] == 1):
                apagado = x[0]
            if x[4] == 0 and apagado is not None and x[0] - apagado > 1_000_000:
                n_fondo += 1
                fondo += x[1] == 0
        print(f"fondo     validas con LED apagado hace > 1 s: {fondo}/{n_fondo}"
              f" ({100 * fondo / max(1, n_fondo):.2f} %)   (es la prueba de que el fondo no da retorno)")
    inv = len(grupos.get("invalido", []))
    saltos = sum(1 for _, k in grupos.get("valido", []) if k == "2 ranuras")
    print(f"criterio  intervalos invalidos >= 1000: {'CUMPLE' if inv >= 1000 else 'NO CUMPLE'} ({inv})"
          f"   |   0 saltos a 2 ranuras en valido: {'CUMPLE' if saltos == 0 else 'NO CUMPLE'} ({saltos})")


if __name__ == "__main__":
    main()
