#!/usr/bin/env python3
"""
Graficas y numeros derivados del barrido v5 del VL53L1X - DASH Timing System

Lee los crudos del logger (sin modificarlos) y genera:
  01_periodo_vs_imp.png       periodo real contra IMP, con las rectas n x ranura
  02_repetibilidad_imp10.png  97 arranques en frio a IMP 10 (intento 1)
e imprime los numeros que usa sensor_vl53l1x_resultados.md.

Uso (desde la raiz del repo, con el venv):
  .venv/bin/python experimentos/sensor_vl53l1x/graficar_sensor.py
"""
import math, os, re, statistics as st
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

TINTA   = "#1a1a1a"
ACENTO  = "#0f6f6a"
ALERTA  = "#c2410c"
SUAVE   = "#94a3b8"

AQUI = os.path.dirname(os.path.abspath(__file__))
V5 = os.path.join(AQUI, "barrido_v5_frio_2026-10-23.txt")
INTENTO1 = os.path.join(AQUI, "barrido_v5_intento1_bucle_2026-10-23.txt")
FACTOR = 0.931       # ranura real = IMP x 0.931 (seccion 9.2)
BUDGET_MS = 20.024   # lo que reporta el chip

plt.rcParams.update({
    "font.size": 10, "axes.edgecolor": SUAVE, "axes.labelcolor": TINTA,
    "xtick.color": TINTA, "ytick.color": TINTA, "axes.spines.top": False,
    "axes.spines.right": False, "figure.dpi": 150,
})

# --- tabla resumen del v5 ---
filas = []
with open(V5, encoding="utf-8", errors="replace") as f:
    en_tabla = False
    for linea in f:
        if "RESUMEN DEL BARRIDO" in linea:
            en_tabla = True
            continue
        if en_tabla and re.match(r"^\d+,", linea):
            c = linea.strip().split(",")
            filas.append(dict(imp=int(c[0]), ranura=float(c[1]), periodo=float(c[2]),
                              n=round(float(c[3])), cociente=float(c[3]),
                              jitter=float(c[4]), anom=int(c[5]), nint=int(c[6]),
                              dist=int(c[8]), validas=c[9]))

# --- intento 1: todas las lineas de periodo (siempre config 1, IMP 10) ---
per_i1 = [float(m.group(1)) for m in
          (re.search(r"periodo ([\d.]+) ms", l) for l in
           open(INTENTO1, encoding="utf-8", errors="replace")) if m]
jit_i1 = [float(m.group(1)) for m in
          (re.search(r"jitter ([\d.]+) us", l) for l in
           open(INTENTO1, encoding="utf-8", errors="replace")) if m]

# --- numeros derivados ---
# T = duracion real de una medicion. El chip usa el minimo n tal que n x ranura >= T.
# Cota inferior: la mayor n x ranura que NO alcanzo (se uso n+1).
# Cota superior: la menor n x ranura que SI alcanzo.
t_inf = max((r["n"] - 1) * r["ranura"] for r in filas if r["n"] > 1)
t_sup = min(r["n"] * r["ranura"] for r in filas)
r25 = next(r for r in filas if r["imp"] == 25)
r12 = next(r for r in filas if r["imp"] == 12)

print(f"configuraciones: {len(filas)}")
print(f"validas totales: {sum(int(r['validas'].split('/')[0]) for r in filas)}/"
      f"{sum(int(r['validas'].split('/')[1]) for r in filas)}")
print(f"anomalos totales: {sum(r['anom'] for r in filas)}/{sum(r['nint'] for r in filas)}")
print(f"distancia: {min(r['dist'] for r in filas)}-{max(r['dist'] for r in filas)} mm")
print(f"cociente periodo/ranura: desviacion max del entero = "
      f"{max(abs(r['cociente'] - r['n']) for r in filas):.3f}")
print(f"duracion real T: {t_inf:.2f} < T <= {t_sup:.2f} ms  (budget {BUDGET_MS} ms)")
print(f"margen a IMP 25: {r25['ranura'] - t_sup:.2f} a {r25['ranura'] - t_inf:.2f} ms")
print(f"margen a IMP 12 (2 ranuras): 0 a {2 * r12['ranura'] - t_inf:.2f} ms")
print(f"cuantizacion IMP 25: {r25['periodo'] / math.sqrt(12):.2f} ms   "
      f"IMP 12: {r12['periodo'] / math.sqrt(12):.2f} ms")
print(f"jitter v5: {min(r['jitter'] for r in filas):.1f}-{max(r['jitter'] for r in filas):.1f} us")
print(f"intento 1: {len(per_i1)} arranques, periodo {min(per_i1):.3f}-{max(per_i1):.3f} ms, "
      f"media {st.mean(per_i1):.3f}, sd {st.pstdev(per_i1) * 1000:.1f} us; "
      f"jitter intra-corrida {min(jit_i1):.1f}-{max(jit_i1):.1f} us")

# --- 01: periodo contra IMP ---
fig, ax = plt.subplots(figsize=(7, 4))
imps = [x / 10 for x in range(95, 260)]
for n, estilo in ((1, "-"), (2, "--"), (3, ":")):
    ax.plot(imps, [n * i * FACTOR for i in imps], estilo, color=SUAVE, lw=1)
    ax.text(25.6, n * 25.6 * FACTOR, f"{n}× ranura", color=SUAVE, va="center", fontsize=8)
ax.axhspan(t_inf, t_sup, color=ALERTA, alpha=0.12, lw=0)
ax.text(13.3, t_inf - 4.5, f"duración real de una medición: {t_inf:.2f}–{t_sup:.2f} ms",
        color=ALERTA, fontsize=8, va="top")
ax.scatter([r["imp"] for r in filas], [r["periodo"] for r in filas], color=ACENTO, zorder=3, s=28)
for r, desp in ((r12, (-4, -16)), (r25, (-90, 8))):
    ax.annotate(f"IMP {r['imp']}: {r['periodo']:.2f} ms", (r["imp"], r["periodo"]),
                textcoords="offset points", xytext=desp, color=TINTA, fontsize=8)
ax.set_xlabel("IMP solicitado (ms)")
ax.set_ylabel("periodo real de INT (ms)")
ax.set_title("Barrido v5 en arranque en frío: el periodo siempre es n × ranura",
             loc="left", color=TINTA, fontsize=10)
ax.set_xlim(9, 28); ax.set_ylim(0, 75)
fig.tight_layout()
fig.savefig(os.path.join(AQUI, "01_periodo_vs_imp.png"))

# --- 02: repetibilidad entre arranques a IMP 10 ---
fig, ax = plt.subplots(figsize=(7, 3))
ax.plot(range(1, len(per_i1) + 1), per_i1, "o", color=ACENTO, ms=3)
ax.axhline(3 * 10 * FACTOR, color=SUAVE, lw=1, ls="--")
ax.text(len(per_i1), 3 * 10 * FACTOR - 0.004, "3 × 9.31 ms", color=SUAVE, fontsize=8, ha="right", va="top")
ax.set_xlabel("arranque en frío #")
ax.set_ylabel("periodo medio (ms)")
ax.set_title(f"IMP 10, {len(per_i1)} arranques seguidos: {min(per_i1):.3f}–{max(per_i1):.3f} ms",
             loc="left", color=TINTA, fontsize=10)
ax.set_ylim(27.90, 28.05)
fig.tight_layout()
fig.savefig(os.path.join(AQUI, "02_repetibilidad_imp10.png"))
