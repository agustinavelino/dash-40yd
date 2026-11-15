#!/usr/bin/env python3
"""Experimento 1: empareja las detecciones del hub y del nodo y calcula sigma.

Uso:
    python3 analizar_exp1.py <crudo_hub.txt> [--csv pares.csv] [--sigma0-us 168]

Lee el crudo del hub (lineas R, H, N, S con checksum *HH; las que fallan el
checksum se descartan y se cuentan).

- Emparejamiento por proximidad temporal (regla 12): la deteccion del nodo se
  pasa al reloj del hub con el offset de la deteccion del hub candidata y se
  une si cae a +-100 ms. Uno a uno, primero los pares mas cercanos.
- resultado = t_hub - (t_nodo - offset)    (formula del Exp. 0, regla 4)
- sigma_sensor = sqrt((sigma_exp1^2 - sigma_exp0^2) / 2)   (Maestro 16.3)
- Intervalo previo a la deteccion (16.3): t[det] - t[det-1] de la traza.
  "normal" ~23.3 ms (1 ranura), "largo" ~33.2 ms (cambio de fase de S2b).

Solo biblioteca estandar (corre con python3 de WSL).
"""
import argparse, csv, math, random, statistics as st, sys

PRE, POST = 10, 5
TRAZA = PRE + 1 + POST
VENTANA_US = 100_000
RANURA_MS = 23.3
LARGO_MS = 33.2
TOL_MS = 3.0
PREDICCIONES = [("Prediccion escrita, ventanas uniformes", 9.5),
                ("Prediccion escrita, con anomalos (9.2)", 10.7),
                ("Hipotesis S2b (ventana de 33.2 ms en cada cruce; NO es prediccion)", 13.6)]


def checksum_ok(linea):
    cuerpo, sep, hh = linea.rpartition("*")
    if not sep or len(hh) != 2:
        return False
    x = 0
    for ch in cuerpo:
        x ^= ord(ch)
    try:
        return x == int(hh, 16)
    except ValueError:
        return False


def traza(campos):
    """4 campos 'a;b;...' -> dict de listas de TRAZA elementos."""
    dt, d, s, m = ([int(v) for v in c.split(";")] for c in campos)
    if not all(len(x) == TRAZA for x in (dt, d, s, m)):
        raise ValueError("traza incompleta")
    return {"dt": dt, "d": d, "s": s, "m": m}


def leer(ruta):
    R, H, N, S = [], [], [], None
    malas = 0
    with open(ruta, encoding="utf-8", errors="replace") as f:
        for linea in f:
            linea = linea.strip()
            if not linea or linea.startswith("#") or linea[0] not in "RHNS":
                continue
            if not checksum_ok(linea):
                malas += 1
                continue
            c = linea.rpartition("*")[0].split(",")
            try:
                if c[0] == "R":
                    R.append({"t": int(c[2]), "offset": int(c[3]), "rtt": int(c[4]), "pongs": int(c[5])})
                elif c[0] == "H":
                    e = {"k": int(c[1]), "ciclo": int(c[2]), "cue": int(c[3]), "t": int(c[4]), "offset": int(c[5])}
                    e.update(traza(c[6:10]))
                    H.append(e)
                elif c[0] == "N":
                    e = {"k": int(c[1]), "seq": int(c[2]), "t": int(c[3]), "t_rx": int(c[4])}
                    e.update(traza(c[8:12]))
                    N.append(e)
                elif c[0] == "S":
                    claves = ["muestras", "rescates", "dobles", "tardias", "det_hub", "det_nodo",
                              "det_hub_fuera", "det_nodo_fuera", "cola_llena", "rafagas"]
                    S = dict(zip(claves, (int(v) for v in c[1:])))
            except (ValueError, IndexError):
                malas += 1
    return R, H, N, S, malas


def emparejar(H, N):
    cand = []
    for i, h in enumerate(H):
        for j, n in enumerate(N):
            dif = h["t"] - (n["t"] - h["offset"])
            if abs(dif) <= VENTANA_US:
                cand.append((abs(dif), i, j, dif))
    cand.sort()
    usadosH, usadosN, pares = set(), set(), []
    for _, i, j, dif in cand:
        if i in usadosH or j in usadosN:
            continue
        usadosH.add(i); usadosN.add(j)
        pares.append((i, j, dif))
    pares.sort()
    return pares, usadosH, usadosN


def intervalo_previo_ms(e):
    return (e["dt"][PRE] - e["dt"][PRE - 1]) / 1000.0


def clase(iv):
    if abs(iv - RANURA_MS) <= TOL_MS:
        return "normal"
    if abs(iv - LARGO_MS) <= TOL_MS:
        return "largo"
    return "otro"


def sigma_ic(valores, n_boot=2000, semilla=1):
    """sigma muestral e intervalo de confianza del 95 % por bootstrap."""
    s = st.stdev(valores)
    rnd = random.Random(semilla)
    bs = sorted(st.stdev(rnd.choices(valores, k=len(valores))) for _ in range(n_boot))
    return s, bs[int(0.025 * n_boot)], bs[int(0.975 * n_boot)]


def fmt_ms(us):
    return f"{us / 1000:.2f} ms"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("crudo")
    ap.add_argument("--csv", help="escribe los pares a este CSV (archivo nuevo)")
    ap.add_argument("--sigma0-us", type=float, default=168.0, help="sigma del Exp. 0 (us)")
    a = ap.parse_args()

    R, H, N, S, malas = leer(a.crudo)
    print(f"Archivo: {a.crudo}")
    print(f"Lineas descartadas por checksum o formato: {malas}")
    if S:
        print(f"Hub: {S['muestras']} muestras, rescates {S['rescates']}, dobles {S['dobles']}, "
              f"tardias {S['tardias']}, cola llena {S['cola_llena']}")
    else:
        print("AVISO: no hay linea S (la corrida no termino o se corto la impresion)")

    # --- resync ---
    if R:
        rtts = [r["rtt"] for r in R if r["rtt"] >= 0]
        print(f"Rafagas de resync: {len(R)}; mejor rtt mediana {st.median(rtts):.0f} us, "
              f"max {max(rtts)} us, > 3 ms: {sum(x > 3000 for x in rtts)}")

    # --- detecciones y referencia del LED ---
    ciclos = {}
    for h in H:
        ciclos.setdefault(h["ciclo"], []).append(h)
    n_ciclos = max((c for c in ciclos if c >= 0), default=-1) + 1
    dobles = sum(1 for c, v in ciclos.items() if c >= 0 and len(v) > 1)
    previas = len(ciclos.get(-1, []))
    print(f"Detecciones: hub {len(H)}, nodo {len(N)} | hub con LED encendido {sum(h['cue'] for h in H)}")
    print(f"Ciclos con al menos una deteccion del hub: {sum(1 for c in ciclos if c >= 0)} de ~{n_ciclos}; "
          f"ciclos con > 1: {dobles}; detecciones durante el parpadeo previo: {previas}")
    seqs = sorted(n["seq"] for n in N)
    if seqs:
        faltan = (seqs[-1] - seqs[0] + 1) - len(seqs)
        print(f"Nodo: seq {seqs[0]}..{seqs[-1]}, mensajes que no llegaron al hub: {faltan}")

    # --- pares ---
    pares, uH, uN = emparejar(H, N)
    res = [dif for _, _, dif in pares]
    print(f"Pares: {len(pares)} | hub sin par {len(H) - len(uH)} | nodo sin par {len(N) - len(uN)}")
    if len(res) < 3:
        print("Muy pocos pares para estadistica.")
        return

    s, lo, hi = sigma_ic(res)
    print(f"\nresultado = t_hub - (t_nodo - offset), n = {len(res)}")
    print(f"  sigma_exp1 = {fmt_ms(s)}  (IC 95 % bootstrap {fmt_ms(lo)} - {fmt_ms(hi)})")
    print(f"  media {fmt_ms(st.mean(res))} | mediana {fmt_ms(st.median(res))} | "
          f"min {fmt_ms(min(res))} | max {fmt_ms(max(res))}")
    s0 = a.sigma0_us
    if s > s0:
        ss = math.sqrt((s ** 2 - s0 ** 2) / 2)
        print(f"  sigma_sensor = sqrt((sigma_exp1^2 - {s0:.0f}^2) / 2) = {fmt_ms(ss)}")
    for nombre, v in PREDICCIONES:
        dentro = "dentro del IC" if lo / 1000 <= v <= hi / 1000 else "fuera del IC"
        print(f"  {nombre}: {v} ms -> {dentro}")

    # --- intervalo previo a la deteccion ---
    print("\nIntervalo previo a la deteccion (t[det] - t[det-1]):")
    for nombre, evs in (("hub", H), ("nodo", N)):
        cl = [clase(intervalo_previo_ms(e)) for e in evs]
        ivs = [intervalo_previo_ms(e) for e in evs]
        print(f"  {nombre}: normal {cl.count('normal')}, largo {cl.count('largo')}, otro {cl.count('otro')} "
              f"| mediana {st.median(ivs):.1f} ms" if ivs else f"  {nombre}: sin detecciones")
    grupos = {}
    for i, j, dif in pares:
        g = (clase(intervalo_previo_ms(H[i])), clase(intervalo_previo_ms(N[j])))
        grupos.setdefault(g, []).append(dif)
    for g, v in sorted(grupos.items(), key=lambda x: -len(x[1])):
        sg = fmt_ms(st.stdev(v)) if len(v) > 2 else "-"
        print(f"  hub {g[0]:6} / nodo {g[1]:6}: n {len(v):3} | sigma {sg} | media {fmt_ms(st.mean(v))}")

    # --- marcas en la deteccion o la muestra previa ---
    def marcado(e):
        return e["m"][PRE] != 0 or e["m"][PRE - 1] != 0
    limpios = [dif for i, j, dif in pares if not marcado(H[i]) and not marcado(N[j])]
    if len(limpios) != len(res):
        print(f"\nPares con marca (rescate/doble/tardia) en det o det-1: {len(res) - len(limpios)}; "
              f"sigma sin ellos {fmt_ms(st.stdev(limpios))}")

    if a.csv:
        with open(a.csv, "x", newline="") as f:
            w = csv.writer(f)
            w.writerow(["par", "ciclo", "cue_hub", "t_hub_us", "t_nodo_us", "offset_us", "resultado_us",
                        "iv_hub_ms", "iv_nodo_ms", "marca_hub", "marca_nodo"])
            for k, (i, j, dif) in enumerate(pares):
                h, n = H[i], N[j]
                w.writerow([k, h["ciclo"], h["cue"], h["t"], n["t"], h["offset"], dif,
                            f"{intervalo_previo_ms(h):.3f}", f"{intervalo_previo_ms(n):.3f}",
                            h["m"][PRE], n["m"][PRE]])
        print(f"\nPares escritos en {a.csv}")


if __name__ == "__main__":
    sys.exit(main())
