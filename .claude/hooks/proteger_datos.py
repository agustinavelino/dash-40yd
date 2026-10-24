#!/usr/bin/env python3
"""
Hook PreToolUse - protege los datos crudos de los experimentos.

Regla del proyecto: los .csv y .txt de experimentos/ son datos crudos;
se crean una vez (los escribe el logger) y no se editan nunca. Una
correccion se hace en el script de analisis, no en el dato.

Bloquea (exit 2):
  - Edit/Write sobre un .csv/.txt EXISTENTE dentro de experimentos/
    (crear uno nuevo si se permite).
  - Bash que modifique uno en sitio: sed -i, truncate, redireccion '>'
    o rm. Renombrar con mv o git mv si se permite (los intentos fallidos
    se renombran, no se borran).
"""
import json, os, re, sys

DATO = r"experimentos/\S*\.(csv|txt)\b"

ev = json.load(sys.stdin)
herramienta = ev.get("tool_name", "")
entrada = ev.get("tool_input", {})
raiz = ev.get("cwd") or os.getcwd()


def bloquear(motivo):
    print(f"BLOQUEADO por .claude/hooks/proteger_datos.py: {motivo}\n"
          "Los datos crudos de experimentos/ no se editan. Corrige en el script "
          "de análisis o guarda una corrida nueva con otro nombre.", file=sys.stderr)
    sys.exit(2)


if herramienta in ("Edit", "Write", "MultiEdit", "NotebookEdit"):
    ruta = entrada.get("file_path", "")
    rel = os.path.relpath(os.path.abspath(ruta), raiz).replace(os.sep, "/")
    if re.fullmatch(DATO, rel) and os.path.exists(ruta):
        bloquear(f"{herramienta} sobre {rel}")

elif herramienta == "Bash":
    cmd = entrada.get("command", "")
    patrones = [
        rf"\bsed\b[^|;&]*\s-i\S*[^|;&]*{DATO}",
        rf"\btruncate\b[^|;&]*{DATO}",
        rf"(?<![>0-9&])>(?!>)\s*\S*{DATO}",
        rf"\brm\b[^|;&]*{DATO}",
    ]
    for p in patrones:
        if re.search(p, cmd):
            bloquear(f"comando que modifica un dato crudo: {cmd[:120]}")

sys.exit(0)
