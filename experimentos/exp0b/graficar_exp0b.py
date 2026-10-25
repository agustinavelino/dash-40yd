#!/usr/bin/env python3
"""
Graficas y numeros del reporte del Experimento 0b - DASH Timing System

Lee los CSV crudos del 0b (corridas de 200 pulsos y prueba de rafagas),
imprime las tablas que usa experimento0b_resultados.md y escribe 4 PNG:
  01_series_escenarios.png   error por evento en cada corrida, con resyncs
  02_rtt_rafagas.png         mejor rtt por rafaga, C0 contra S3 (prueba de rafagas)
  03_error_vs_rtt.png        error real del offset contra mejor rtt, con la cota
  04_pings_todos.png         round-trip de TODOS los pings (piso F)

Uso (desde la raiz del repo):  .venv/bin/python experimentos/exp0b/graficar_exp0b.py
"""
import math, os, statistics as st, sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(AQUI, "..", "..", "tools"))
sys.path.insert(0, AQUI)
from resumen_corrida import leer as leer_eventos, checksum_ok
from analizar_rafagas import leer as leer_rafagas, F_US, PERIODO_S   # sin argumentos no analiza nada

TINTA   = "#1a1a1a"
ACENTO  = "#0f6f6a"
ALERTA  = "#c2410c"
SUAVE   = "#94a3b8"

CORRIDAS = [   # (etiqueta, archivo); orden cronologico del 23-oct-2026
    ("C0 (1)", "exp0b_C0_2026-10-23.csv"),
    ("S1",     "exp0b_S1_2026-10-23.csv"),
    ("S2",     "exp0b_S2_2026-10-23.csv"),
    ("S3 (1)", "exp0b_S3_2026-10-23.csv"),
    ("S3 (2)", "exp0b_S3_2026-10-23_2.csv"),
    ("C0 (2)", "exp0b_C0_2026-10-23_2.csv"),
]
RAF = [("R-C0", "exp0b_rafagas_C0_2026-10-23.csv"),
       ("R-S3", "exp0b_rafagas_S3_2026-10-23.csv")]


def pings(ruta):
    """Round-trips de todos los pings (lineas R), primera copia valida. -1 = perdido."""
    d = {}
    with open(ruta, encoding="latin-1") as f:
        for linea in f:
            s = linea.strip()
            if not s.startswith("R,"):
                continue
            datos, ok = checksum_ok(s)
            c = datos.split(",")
            if ok and len(c) == 4:
                try:
                    d.setdefault((int(c[1]), int(c[2])), int(c[3]))
                except ValueError:
                    pass
    return d


def fisher_2colas(a, n1, b, n2):
    """Prueba exacta de Fisher, dos colas, para a/n1 contra b/n2."""
    k, N = a + b, n1 + n2
    p = lambda x: math.comb(n1, x) * math.comb(n2, k - x) / math.comb(N, k)
    p0 = p(a)
    return sum(p(x) for x in range(max(0, k - n2), min(k, n1) + 1) if p(x) <= p0 * (1 + 1e-9))


def mann_whitney(x, y):
    """U de Mann-Whitney, aproximacion normal con correccion por empates. Devuelve p de dos colas."""
    todos = sorted([(v, 0) for v in x] + [(v, 1) for v in y])
    rangos, i = [0.0] * len(todos), 0
    empates = 0
    while i < len(todos):
        j = i
        while j + 1 < len(todos) and todos[j + 1][0] == todos[i][0]:
            j += 1
        for k in range(i, j + 1):
            rangos[k] = (i + j) / 2 + 1
        t = j - i + 1
        empates += t ** 3 - t
        i = j + 1
    n1, n2 = len(x), len(y)
    r1 = sum(r for r, (_, g) in zip(rangos, todos) if g == 0)
    u = r1 - n1 * (n1 + 1) / 2
    N = n1 + n2
    sd = math.sqrt(n1 * n2 / 12 * ((N + 1) - empates / (N * (N - 1))))
    z = (u - n1 * n2 / 2) / sd
    return math.erfc(abs(z) / math.sqrt(2))


# ---------------- corridas de 200 pulsos ----------------
series = []
print("== Corridas de 200 pulsos")
print("corrida | eventos | corruptas | sigma | media | rtt med | pongs | clientes | paginas/s")
for nombre, archivo in CORRIDAS:
    filas, corruptas = leer_eventos(os.path.join(AQUI, archivo))
    filas.sort()
    seq = [r[0] for r in filas]; res = [r[1] for r in filas]
    off = [r[2] for r in filas]; rtt = [r[3] for r in filas]
    pag = f"{(filas[-1][6] - filas[0][6]) / ((seq[-1] - seq[0]) * .5):.2f}" if len(filas[0]) >= 7 else "-"
    print(f"{nombre} | {len(seq)}/200 | {corruptas} | {st.pstdev(res):.0f} | {st.mean(res):.0f} | "
          f"{st.median(rtt):.0f} | {st.mean(r[4] for r in filas):.2f} | "
          f"{min(r[5] for r in filas)}-{max(r[5] for r in filas)} | {pag}")
    # bloques entre resyncs: el offset cambia en cada uno
    inicios = [0] + [i for i in range(1, len(off)) if off[i] != off[i - 1]]
    bloques = []
    for b, i in enumerate(inicios):
        bloques.append((b + 1, seq[i], rtt[i], (rtt[i] - F_US) / 2, res[i]))
    print("   bloque: (n, seq inicial, mejor rtt, cota, error al inicio)  ",
          "  ".join(f"[{n}] seq {s} rtt {r} cota ±{c:.0f} err {e:+d}" for n, s, r, c, e in bloques))
    p = pings(os.path.join(AQUI, archivo))
    vals = [v for v in p.values() if v >= 0]
    print(f"   pings: {len(vals)}/{len(p)} llegaron, min {min(vals)} us, mediana {st.median(vals):.0f} us")
    series.append((nombre, seq, res, inicios, rtt, st.pstdev(res)))

# ---------------- prueba de rafagas ----------------
print("\n== Prueba de rafagas")
raf = {}
for nombre, archivo in RAF:
    r = [x for x in leer_rafagas(os.path.join(AQUI, archivo)) if x["rtt"] >= 0]
    t = [x["n"] * PERIODO_S for x in r]; o = [x["off"] for x in r]
    mt, mo = st.mean(t), st.mean(o)
    m = sum((a - mt) * (b - mo) for a, b in zip(t, o)) / sum((a - mt) ** 2 for a in t)
    resid = [b - (mo + m * (a - mt)) for a, b in zip(t, o)]
    rtt = [x["rtt"] for x in r]
    p = pings(os.path.join(AQUI, archivo))
    todos = [v for v in p.values() if v >= 0]
    lentas = sum(v > 3000 for v in rtt)
    raf[nombre] = dict(rtt=rtt, resid=resid, todos=todos, lentas=lentas, n=len(r))
    print(f"{nombre}: {len(r)} rafagas, lentas {lentas}, mejor rtt mediana {st.median(rtt):.0f}, "
          f"deriva {m:.1f} us/s, error sigma {st.pstdev(resid):.0f} max {max(map(abs, resid)):.0f} us")
    print(f"   pings {len(todos)}/{len(p)} llegaron, min {min(todos)} us (F Exp. 0 = {F_US}), "
          f"mediana {st.median(todos):.0f}, p90 {sorted(todos)[int(.9 * len(todos))]}, "
          f"> 3 ms {100 * sum(v > 3000 for v in todos) / len(todos):.0f} %")
c0, s3 = raf["R-C0"], raf["R-S3"]
print(f"Fisher (lentas {c0['lentas']}/{c0['n']} vs {s3['lentas']}/{s3['n']}): p = {fisher_2colas(c0['lentas'], c0['n'], s3['lentas'], s3['n']):.4f}")
print(f"Mann-Whitney mejor rtt: p = {mann_whitney(c0['rtt'], s3['rtt']):.2e}")
print(f"Mann-Whitney todos los pings: p = {mann_whitney(c0['todos'], s3['todos']):.2e}")

plt.rcParams.update({
    "font.size": 10, "axes.edgecolor": SUAVE, "axes.labelcolor": TINTA,
    "text.color": TINTA, "xtick.color": TINTA, "ytick.color": TINTA,
    "axes.spines.top": False, "axes.spines.right": False, "figure.dpi": 140,
})
os.chdir(AQUI)

# ---------- 1. Series por escenario ----------
fig, axs = plt.subplots(2, 3, figsize=(12, 6), sharex=True, sharey=True)
for ax, (nombre, seq, res, inicios, rtt, sigma) in zip(axs.flat, series):
    ax.plot(seq, res, lw=1.2, color=ACENTO, zorder=3)
    ax.axhline(0, color=SUAVE, lw=1, ls="--", zorder=1)
    for i in inicios[1:]:
        lenta = rtt[i] > 3000
        ax.axvline(seq[i], color=ALERTA if lenta else SUAVE, lw=1, ls=":", zorder=2)
    lentas = [k + 1 for k, i in enumerate(inicios) if rtt[i] > 3000]
    ax.set_title(f"{nombre}   $\\sigma$ = {sigma:.0f} $\\mu$s", fontsize=10)
    if lentas:
        ax.text(.02, .96, "rafaga lenta: " + ", ".join(map(str, lentas)), transform=ax.transAxes,
                fontsize=8, color=ALERTA, va="top")
for ax in axs[1]:
    ax.set_xlabel("Evento (pulso #)")
for ax in axs[:, 0]:
    ax.set_ylabel("Error  [$\\mu$s]")
fig.suptitle("Experimento 0b — error por evento (resync punteado; naranja = mejor rtt > 3 ms)")
fig.tight_layout(); fig.savefig("01_series_escenarios.png"); plt.close(fig)

# ---------- 2. Mejor rtt por rafaga (ECDF) ----------
fig, ax = plt.subplots(figsize=(8, 4.5))
for nombre, color in (("R-C0", ACENTO), ("R-S3", ALERTA)):
    v = sorted(raf[nombre]["rtt"])
    ax.step(v, [(i + 1) / len(v) for i in range(len(v))], where="post", color=color, lw=1.6,
            label=f"{nombre}: mediana {st.median(v):.0f} $\\mu$s, > 3 ms {raf[nombre]['lentas']} %")
ax.axvline(3000, color=SUAVE, lw=1, ls="--")
ax.axvline(F_US, color=SUAVE, lw=1, ls=":")
ax.text(F_US, .02, " F", color=TINTA, fontsize=9)
ax.set_xlabel("Mejor round-trip de la rafaga  [$\\mu$s]")
ax.set_ylabel("Fraccion acumulada de rafagas")
ax.set_title("Prueba de rafagas — 100 rafagas por condicion", pad=12)
ax.legend(frameon=False, loc="lower right")
fig.tight_layout(); fig.savefig("02_rtt_rafagas.png"); plt.close(fig)

# ---------- 3. Error real contra mejor rtt ----------
fig, ax = plt.subplots(figsize=(8, 4.5))
xmax = max(max(c0["rtt"]), max(s3["rtt"])) + 200
ax.fill_between([F_US, xmax], [0, -(xmax - F_US) / 2], [0, (xmax - F_US) / 2],
                color=SUAVE, alpha=.18, lw=0, label="cota $\\pm$(rtt $-$ F)/2")
for nombre, color, marca in (("R-C0", ACENTO, "o"), ("R-S3", ALERTA, "^")):
    ax.scatter(raf[nombre]["rtt"], raf[nombre]["resid"], s=16, color=color, marker=marca,
               label=nombre, zorder=3)
ax.axvline(3000, color=SUAVE, lw=1, ls="--")
ax.set_xlim(F_US - 100, xmax)
ax.set_xlabel("Mejor round-trip de la rafaga  [$\\mu$s]")
ax.set_ylabel("Error real del offset  [$\\mu$s]")
ax.set_title("El error del offset lo explica el round-trip, con o sin WiFi", pad=12)
ax.legend(frameon=False, loc="lower left")
fig.tight_layout(); fig.savefig("03_error_vs_rtt.png"); plt.close(fig)

# ---------- 4. Todos los pings ----------
fig, ax = plt.subplots(figsize=(8, 4.5))
bins = [2200 + 200 * i for i in range(40)]
for nombre, color in (("R-C0", ACENTO), ("R-S3", ALERTA)):
    v = raf[nombre]["todos"]
    ax.hist([min(x, bins[-1] - 1) for x in v], bins=bins, histtype="step", lw=1.6, color=color,
            label=f"{nombre}: n = {len(v)}, min {min(v)} $\\mu$s, mediana {st.median(v):.0f} $\\mu$s")
ax.axvline(F_US, color=SUAVE, lw=1, ls=":")
ax.set_xlabel("Round-trip de cada ping  [$\\mu$s]  (ultima barra = 9 800 o mas)")
ax.set_ylabel("Pings")
ax.set_title("Todos los pings de la prueba de rafagas — el piso F no se mueve", pad=12)
ax.legend(frameon=False)
fig.tight_layout(); fig.savefig("04_pings_todos.png"); plt.close(fig)
