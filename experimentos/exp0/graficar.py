#!/usr/bin/env python3
"""
Graficas del Experimento 0 - DASH Timing System
Lee datos_raw.csv (seq,resultado_us,offset_us) y genera 3 PNG.
"""
import csv, statistics as st
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

TINTA   = "#1a1a1a"
ACENTO  = "#0f6f6a"
ALERTA  = "#c2410c"
SUAVE   = "#94a3b8"

seq, res, off = [], [], []
with open("datos_raw.csv") as f:
    for row in csv.reader(f):
        if len(row) < 3 or not row[0].strip().isdigit():
            continue
        seq.append(int(row[0])); res.append(int(row[1])); off.append(int(row[2]))

sigma = st.pstdev(res)
media = st.mean(res)

# frontera de cada resync = donde cambia el offset
fronteras = [seq[i] for i in range(1, len(off)) if off[i] != off[i-1]]

plt.rcParams.update({
    "font.size": 10, "axes.edgecolor": SUAVE, "axes.labelcolor": TINTA,
    "text.color": TINTA, "xtick.color": TINTA, "ytick.color": TINTA,
    "axes.spines.top": False, "axes.spines.right": False, "figure.dpi": 140,
})

# ---------- 1. Serie temporal (diente de sierra) ----------
fig, ax = plt.subplots(figsize=(10, 4.5))
ax.plot(seq, res, lw=1.4, color=ACENTO, zorder=3)
ax.axhline(0, color=SUAVE, lw=1, ls="--", zorder=1)
for i, x in enumerate(fronteras):
    ax.axvline(x, color=ALERTA, lw=1, ls=":", alpha=.8, zorder=2,
               label="resync (cada 30 s)" if i == 0 else None)
ax.set_xlabel("Evento (pulso #)")
ax.set_ylabel("Error de sincronizacion  [$\\mu$s]")
ax.set_title("Experimento 0 — deriva de cristal corregida por resync", pad=12)
ax.legend(frameon=False, loc="lower left")
ax.text(.985, .06, f"$\\sigma$ = {sigma:.0f} $\\mu$s\nmedia = {media:.0f} $\\mu$s",
        transform=ax.transAxes, ha="right", va="bottom", fontsize=9,
        bbox=dict(boxstyle="round,pad=0.45", fc="white", ec=SUAVE, lw=.8))
fig.tight_layout(); fig.savefig("01_serie_temporal.png"); plt.close(fig)

# ---------- 2. Histograma ----------
fig, ax = plt.subplots(figsize=(8, 4.5))
ax.hist(res, bins=24, color=ACENTO, edgecolor="white", lw=.7, zorder=3)
ax.axvline(0, color=SUAVE, lw=1.2, ls="--", zorder=2, label="valor verdadero (0)")
ax.axvline(media, color=ALERTA, lw=1.4, zorder=4, label=f"media = {media:.0f} $\\mu$s")
ax.set_xlabel("Error de sincronizacion  [$\\mu$s]")
ax.set_ylabel("Frecuencia")
ax.set_title(f"Distribucion del error  (n = {len(res)},  $\\sigma$ = {sigma:.0f} $\\mu$s)", pad=12)
ax.legend(frameon=False)
fig.tight_layout(); fig.savefig("02_histograma.png"); plt.close(fig)

# ---------- 3. Presupuesto de error ----------
fuentes = ["Geometria\ncorporal", "Cuantizacion\nsensor 20 ms",
           "Sincronizacion\n(MEDIDO)", "Deriva de\ncristal", "ISR +\nfirmware"]
valores = [35000, 10000, sigma, 200, 1.3]
colores = [SUAVE, SUAVE, ACENTO, SUAVE, SUAVE]

fig, ax = plt.subplots(figsize=(8.5, 4.5))
barras = ax.barh(fuentes, valores, color=colores, height=.62, zorder=3)
ax.set_xscale("log")
ax.set_xlabel("Contribucion al error, 1$\\sigma$  [$\\mu$s, escala log]")
ax.set_title("Presupuesto de error — donde cae la sincronizacion medida", pad=12)
ax.invert_yaxis()
for b, v in zip(barras, valores):
    txt = f"{v:,.0f}" if v >= 10 else f"{v:.1f}"
    ax.text(v * 1.18, b.get_y() + b.get_height()/2, txt + " $\\mu$s",
            va="center", fontsize=9)
ax.set_xlim(.5, 200000)
fig.tight_layout(); fig.savefig("03_presupuesto_error.png"); plt.close(fig)

print(f"n={len(res)}  sigma={sigma:.1f} us  media={media:.1f} us")
print(f"resyncs detectados en seq: {fronteras}")
print("generados: 01_serie_temporal.png  02_histograma.png  03_presupuesto_error.png")
