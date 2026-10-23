#!/usr/bin/env python3
"""
Logger serial - DASH Timing System

Guarda TODO lo que llega por el puerto a un archivo, linea por linea y
sin tocarlo (dato crudo). Termina al ver el marcador de fin, al pasar
--max-s segundos o tras --idle-s segundos sin recibir nada.

IMPORTANTE (WSL2): este script corre con el Python de WINDOWS, que es
el unico que ve los puertos COM. Desde WSL:

  PYW=/mnt/c/Users/aagus/AppData/Local/Programs/Python/Python311/python.exe
  $PYW "$(wslpath -w tools/logger_serial.py)" --puerto COM11 \
       --salida "$(wslpath -w experimentos/exp0b/exp0b_C0.csv)" \
       --fin "FIN DE CORRIDA" --max-s 300

Abrir el puerto reinicia el ESP32 (DTR/RTS): la corrida arranca desde
el boot, que es lo que se quiere. Por eso el logger se lanza ANTES de
que empiece la corrida y no a medias. Solo un programa puede tener el
puerto abierto: cerrar el monitor del Arduino IDE.
"""
import argparse, re, sys, time

import serial


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--puerto", required=True)
    ap.add_argument("--baud", type=int, default=921600)   # regla 5 del CLAUDE.md
    ap.add_argument("--salida", required=True)
    ap.add_argument("--fin", default=None, help="regex que marca el fin de la corrida")
    ap.add_argument("--max-s", type=float, default=600)
    ap.add_argument("--idle-s", type=float, default=120)
    a = ap.parse_args()

    fin = re.compile(a.fin) if a.fin else None
    t0 = time.time()
    t_ultimo = t0
    n = 0
    motivo = "max-s"

    with serial.Serial(a.puerto, a.baud, timeout=0.5) as s, \
         open(a.salida, "w", encoding="utf-8", newline="\n") as f:
        while time.time() - t0 < a.max_s:
            crudo = s.readline()
            if not crudo:
                if time.time() - t_ultimo > a.idle_s:
                    motivo = "idle-s"
                    break
                continue
            t_ultimo = time.time()
            linea = crudo.decode("utf-8", errors="replace").rstrip("\r\n")
            f.write(linea + "\n")
            f.flush()                 # si se corta la corrida, lo recibido ya esta en disco
            n += 1
            if fin and fin.search(linea):
                motivo = "fin"
                break

    # Resumen de una linea: es lo unico que el agente necesita leer.
    print(f"logger: {n} lineas en {time.time() - t0:.0f} s, termino por {motivo} -> {a.salida}")
    sys.exit(0 if motivo == "fin" or not fin else 2)


if __name__ == "__main__":
    main()
