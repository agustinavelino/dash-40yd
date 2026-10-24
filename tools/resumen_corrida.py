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


def checksum_ok(linea):
    """Lineas del 0b en adelante: 'datos*HH' con HH = XOR de los bytes de datos.
    Devuelve (datos, ok). Una linea sin '*' (formato del Exp. 0) se acepta tal cual."""
    if "*" not in linea:
        return linea, True
    datos, _, hh = linea.rpartition("*")
    x = 0
    for c in datos.encode("latin-1", errors="replace"):
        x ^= c
    return datos, hh.strip().upper() == f"{x:02X}"


def leer(ruta):
    """Devuelve (filas, corruptas). Si la tabla viene repetida (0b: dos copias),
    se queda con la primera copia VALIDA de cada seq."""
    filas, vistos, corruptas = [], set(), 0
    with open(ruta, encoding="latin-1", newline="") as f:
        for linea in f:
            linea = linea.strip()
            if not linea[:1].isdigit():
                continue
            datos, ok = checksum_ok(linea)
            row = datos.split(",")
            try:
                valores = [int(x) for x in row]
            except ValueError:
                if ok and len(row) >= 2 and row[1] == "FALTA":
                    continue      # el hub no tuvo el evento: perdida real
                corruptas += 1    # linea truncada o con basura
                continue
            if not ok:
                corruptas += 1
                continue
            if valores[0] in vistos:
                continue          # segunda copia de un seq ya leido
            vistos.add(valores[0])
            filas.append(valores)
    return filas, corruptas


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--esperados", type=int, default=200)
    a = ap.parse_args()

    filas, corruptas = leer(a.csv)
    if not filas:
        print(f"SIN DATOS en {a.csv}")
        sys.exit(1)

    seqs = {r[0] for r in filas}
    res = [r[1] for r in filas]
    faltan = sorted(set(range(1, a.esperados + 1)) - seqs)

    # pstdev, igual que graficar.py del Exp. 0, para que los numeros sean comparables
    print(f"archivo   {a.csv}")
    print(f"eventos   {len(seqs)}/{a.esperados}  perdidos {len(faltan)}"
          + (f"  (seq {faltan[:10]}{'...' if len(faltan) > 10 else ''})" if faltan else "")
          + (f"  | lineas corruptas descartadas {corruptas}" if corruptas else ""))
    print(f"sigma     {st.pstdev(res):.0f} us   media {st.mean(res):.0f} us"
          f"   min {min(res)}  max {max(res)} us")
    print(f"REQ-NF-02 (sigma <= 5 ms): {'CUMPLE' if st.pstdev(res) <= 5000 else 'NO CUMPLE'}")

    # Columnas extra del 0b: seq,resultado_us,offset_us,rtt_us,pongs_recibidos
    if all(len(r) >= 5 for r in filas):
        rtt = [r[3] for r in filas]
        pongs = [r[4] for r in filas]
        print(f"rtt elegido  mediana {st.median(rtt):.0f} us (Exp. 0: 2483)"
              f"   pongs promedio {st.mean(pongs):.2f}/7")
    # 0b: clientes WebSocket al procesar cada evento (verifica el escenario)
    if all(len(r) >= 6 for r in filas):
        cli = [r[5] for r in filas]
        print(f"clientes_ws  min {min(cli)}  max {max(cli)}  mediana {st.median(cli):.0f}")
    # 0b: descargas acumuladas de la pagina (la carga real de S3)
    if all(len(r) >= 7 for r in filas):
        f = sorted(filas)
        dp, ds = f[-1][6] - f[0][6], (f[-1][0] - f[0][0]) * 0.5   # un pulso cada 0.5 s
        print(f"paginas      {dp} descargas en {ds:.0f} s de corrida = {dp / ds:.2f}/s (S3 nominal: 1/s)")


if __name__ == "__main__":
    main()
