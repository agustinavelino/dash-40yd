#!/usr/bin/env python3
"""
Resumen corto de una corrida tipo Experimento 0 / 0b - DASH Timing System

Lee un CSV cuya primera columna es seq y la segunda resultado_us (el
formato de exp0_datos.csv; el 0b agrega rtt_us y pongs_recibidos al
final). Ignora cualquier linea que no empiece con un numero, asi que
acepta tal cual la salida serial cruda del logger.

Imprime solo unas lineas: lo que el agente de banco devuelve a la
conversacion. Las graficas y el analisis largo son trabajo del reporte.

Uso:  python3 tools/resumen_corrida.py <archivo.csv> [--esperados 200]
"""
import argparse, csv, statistics as st, sys


def leer(ruta):
    filas = []
    with open(ruta, newline="") as f:
        for row in csv.reader(f):
            if len(row) < 2 or not row[0].strip().isdigit():
                continue
            try:
                filas.append([int(x) for x in row])
            except ValueError:
                continue          # linea truncada o corrupta: se cuenta como perdida
    return filas


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--esperados", type=int, default=200)
    a = ap.parse_args()

    filas = leer(a.csv)
    if not filas:
        print(f"SIN DATOS en {a.csv}")
        sys.exit(1)

    seqs = {r[0] for r in filas}
    res = [r[1] for r in filas]
    faltan = sorted(set(range(1, a.esperados + 1)) - seqs)

    # pstdev, igual que graficar.py del Exp. 0, para que los numeros sean comparables
    print(f"archivo   {a.csv}")
    print(f"eventos   {len(seqs)}/{a.esperados}  perdidos {len(faltan)}"
          + (f"  (seq {faltan[:10]}{'...' if len(faltan) > 10 else ''})" if faltan else ""))
    print(f"sigma     {st.pstdev(res):.0f} us   media {st.mean(res):.0f} us"
          f"   min {min(res)}  max {max(res)} us")
    print(f"REQ-NF-02 (sigma <= 5 ms): {'CUMPLE' if st.pstdev(res) <= 5000 else 'NO CUMPLE'}")

    # Columnas extra del 0b: seq,resultado_us,offset_us,rtt_us,pongs_recibidos
    if all(len(r) >= 5 for r in filas):
        rtt = [r[3] for r in filas]
        pongs = [r[4] for r in filas]
        print(f"rtt elegido  mediana {st.median(rtt):.0f} us (Exp. 0: 2483)"
              f"   pongs promedio {st.mean(pongs):.2f}/7")


if __name__ == "__main__":
    main()
