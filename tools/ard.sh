#!/usr/bin/env bash
# Wrapper de arduino-cli para WSL2 - DASH Timing System
#
# arduino-cli.exe es de Windows: recibe rutas de Windows y es el unico
# que ve los COM. Este wrapper convierte la ruta del sketch y fija el
# FQBN, para que nadie tenga que repetir rutas largas.
#
#   tools/ard.sh compile <carpeta_sketch>
#   tools/ard.sh upload  <carpeta_sketch> <COMn>   (compila y sube)
#   tools/ard.sh boards
#
# NO verifica que el puerto sea la placa correcta: eso se hace antes
# con tools/identificar_puertos.py (lo exige el runner).
set -euo pipefail

CLI="/mnt/c/Users/aagus/AppData/Local/Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"
FQBN="esp32:esp32:esp32"

# Solo errores, avisos y el tamano: la salida completa de esptool satura el contexto
filtro() { grep -E 'error|warning:|Sketch uses|Global variables|Hash of data verified|Hard resetting|Error' || true; }

case "${1:-}" in
  compile)
    "$CLI" compile --fqbn "$FQBN" "$(wslpath -w "$2")" 2>&1 | filtro
    exit "${PIPESTATUS[0]}" ;;
  upload)
    [[ "${3:-}" =~ ^COM[0-9]+$ ]] || { echo "uso: ard.sh upload <sketch> <COMn>"; exit 1; }
    "$CLI" compile --upload -p "$3" --fqbn "$FQBN" "$(wslpath -w "$2")" 2>&1 | filtro
    exit "${PIPESTATUS[0]}" ;;
  boards)
    "$CLI" board list ;;
  *)
    sed -n '2,12p' "$0"; exit 1 ;;
esac
