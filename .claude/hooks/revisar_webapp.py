#!/usr/bin/env python3
"""
Hook PostToolUse - revisa la pagina de producto en webapp/.

REQ-NF-08 / seccion 13.3: el telefono en el SoftAP no tiene internet,
asi que no puede haber ningun recurso externo, y la pagina debe ser del
orden de 20 KB. Despues de cada Edit/Write en webapp/ revisa:
  - referencias a recursos externos (src/href/url()/import/fetch a http(s):// o //)
  - tamano > 30 KB sin comprimir (aviso, no error de diseno)
Si encuentra algo, exit 2: el aviso le llega a Claude para corregirlo.
No aplica a la pagina de prueba pesada del Exp. 0b (vive en experimentos/).
"""
import json, os, re, sys

ev = json.load(sys.stdin)
ruta = ev.get("tool_input", {}).get("file_path", "")
raiz = ev.get("cwd") or os.getcwd()
rel = os.path.relpath(os.path.abspath(ruta), raiz).replace(os.sep, "/")

if not rel.startswith("webapp/") or not os.path.isfile(ruta):
    sys.exit(0)
if not rel.endswith((".html", ".htm", ".css", ".js")):
    sys.exit(0)

texto = open(ruta, encoding="utf-8", errors="replace").read()
problemas = []

externo = re.compile(
    r"""(?:src|href|action)\s*=\s*["']?\s*(?:https?:)?//"""
    r"""|url\(\s*["']?\s*(?:https?:)?//"""
    r"""|@import\s+(?:url\()?\s*["']?(?:https?:)?//"""
    r"""|\b(?:fetch|import)\s*\(\s*["'`](?:https?:)?//""",
    re.I)
for n, linea in enumerate(texto.splitlines(), 1):
    if externo.search(linea):
        problemas.append(f"  línea {n}: recurso externo -> {linea.strip()[:100]}")

kb = len(texto.encode("utf-8")) / 1024
if kb > 30:
    problemas.append(f"  tamaño {kb:.1f} KB sin comprimir (objetivo ~20 KB, sección 13.3)")

if problemas:
    print(f"webapp: {rel} no cumple REQ-NF-08 / 13.3:\n" + "\n".join(problemas),
          file=sys.stderr)
    sys.exit(2)
sys.exit(0)
