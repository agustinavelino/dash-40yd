#!/usr/bin/env python3
"""Grafica de la validacion del filtro de latencia minima (prueba 3)."""
import csv, statistics as st, math
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

TINTA, ACENTO, ALERTA, SUAVE = "#1a1a1a", "#0f6f6a", "#c2410c", "#94a3b8"

r, f, dm, c, dc = [], [], [], [], []
with open("p3.csv") as fh:
    rd = csv.reader(fh); next(rd)
    for row in rd:
        r.append(int(row[0])); f.append(int(row[1])); dm.append(int(row[2]))
        c.append(int(row[3])); dc.append(int(row[4]))

def detrend(xs, ys):
    n = len(xs); sx = sum(xs); sy = sum(ys)
    sxy = sum(a*b for a, b in zip(xs, ys)); sxx = sum(a*a for a in xs)
    m = (n*sxy - sx*sy) / (n*sxx - sx*sx); b = (sy - m*sx) / n
    return [y - (m*x + b) for x, y in zip(xs, ys)]

rf, rc = detrend(r, f), detrend(r, c)
sf, sc = st.pstdev(rf), st.pstdev(rc)

plt.rcParams.update({
    "font.size": 10, "axes.edgecolor": SUAVE, "text.color": TINTA,
    "xtick.color": TINTA, "ytick.color": TINTA, "axes.labelcolor": TINTA,
    "axes.spines.top": False, "axes.spines.right": False, "figure.dpi": 140,
})

fig, (a1, a2) = plt.subplots(1, 2, figsize=(12, 4.6))

# --- panel izquierdo: dispersion del offset ---
a1.plot(r, rc, lw=1.1, color=ALERTA, alpha=.85, label=f"1 ping-pong  ($\\sigma$={sc:.0f} $\\mu$s)")
a1.plot(r, rf, lw=1.4, color=ACENTO, label=f"mejor de 7  ($\\sigma$={sf:.0f} $\\mu$s)")
a1.axhline(0, color=SUAVE, lw=.9, ls="--")
a1.set_xlabel("Ronda de resync"); a1.set_ylabel("Desviacion del offset  [$\\mu$s]")
a1.set_title(f"Filtro de latencia minima: {sc/sf:.1f}$\\times$ menos ruido", pad=10)
a1.legend(frameon=False, fontsize=9)

# --- panel derecho: delay vs error ---
dev = [abs(x) for x in rc]
mx, my = st.mean(dc), st.mean(dev)
num = sum((a-mx)*(b-my) for a, b in zip(dc, dev))
den = math.sqrt(sum((a-mx)**2 for a in dc) * sum((b-my)**2 for b in dev))
rho = num/den

a2.scatter(dc, dev, s=26, color=ALERTA, alpha=.6, edgecolor="none", zorder=3)
piso = min(dm)
a2.axvline(piso, color=SUAVE, lw=1, ls=":", zorder=2)
a2.text(piso*1.06, max(dev)*.93, f"piso fisico\n{piso} $\\mu$s", fontsize=8, color=TINTA)
a2.set_xlabel("Round-trip medido  [$\\mu$s]")
a2.set_ylabel("|error del offset|  [$\\mu$s]")
a2.set_title(f"Round-trip largo $\\Rightarrow$ offset peor  ($\\rho$ = {rho:.2f})", pad=10)

fig.tight_layout(); fig.savefig("04_filtro_latencia.png"); plt.close(fig)
print(f"crudo sigma={sc:.0f}  filtrado sigma={sf:.0f}  mejora={sc/sf:.2f}x  rho={rho:.2f}")
