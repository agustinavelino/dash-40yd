#!/usr/bin/env python3
"""
Genera la pagina de prueba pesada del Experimento 0b - DASH Timing System

Toma pagina_prueba/pagina_prueba.html, reemplaza <!--LASTRE--> por un
bloque de datos pseudoaleatorios en base64 (no se ejecuta; solo pesa),
la comprime con gzip y escribe prueba_0b_hub/pagina_gz.h.

Por que datos aleatorios: el plan pide una pagina de "cientos de KB"
servida con gzip. Un relleno repetitivo se comprimiria a casi nada y no
cargaria el radio; el aleatorio en base64 solo baja a ~75 %, asi que lo
que viaja por el aire es realmente del orden de lo que se pide.
Semilla fija: la misma pagina byte a byte en cada generacion.

Uso:  python3 experimentos/exp0b/generar_pagina.py [--kb 300]
"""
import argparse, base64, gzip, os, random

AQUI = os.path.dirname(os.path.abspath(__file__))
PLANTILLA = os.path.join(AQUI, "pagina_prueba", "pagina_prueba.html")
SALIDA = os.path.join(AQUI, "prueba_0b_hub", "pagina_gz.h")

ap = argparse.ArgumentParser()
ap.add_argument("--kb", type=int, default=300, help="tamano del lastre en KB (base64)")
a = ap.parse_args()

rnd = random.Random(20261023)
crudo = bytes(rnd.getrandbits(8) for _ in range(a.kb * 1024 * 3 // 4))
lastre = base64.b64encode(crudo).decode()
lineas = "\n".join(lastre[i:i + 120] for i in range(0, len(lastre), 120))
bloque = f'<script id="lastre" type="application/octet-stream">\n{lineas}\n</script>'

html = open(PLANTILLA, encoding="utf-8").read()
assert "<!--LASTRE-->" in html
html = html.replace("<!--LASTRE-->", bloque).encode("utf-8")
gz = gzip.compress(html, compresslevel=9, mtime=0)   # mtime=0: salida reproducible

with open(SALIDA, "w", encoding="utf-8", newline="\n") as f:
    f.write("// GENERADO por experimentos/exp0b/generar_pagina.py - no editar a mano\n")
    f.write(f"// html {len(html)} B -> gzip {len(gz)} B (lastre {a.kb} KB)\n")
    f.write("#pragma once\n#include <Arduino.h>\n\n")
    f.write(f"const size_t PAGINA_GZ_LEN = {len(gz)};\n")
    f.write("const uint8_t PAGINA_GZ[] PROGMEM = {\n")
    for i in range(0, len(gz), 24):
        f.write("  " + ",".join(f"0x{b:02x}" for b in gz[i:i + 24]) + ",\n")
    f.write("};\n")

print(f"html {len(html) / 1024:.0f} KB -> gzip {len(gz) / 1024:.0f} KB  ->  {os.path.relpath(SALIDA)}")
