#!/usr/bin/env python3
"""
Identifica que puerto COM es el hub y cual el nodo - DASH Timing System

Lee la MAC de fabrica (eFuse) de cada puerto con el esptool que trae el
core esp32. No depende del firmware cargado, asi que funciona aunque la
placa tenga cualquier sketch o ninguno. Efecto secundario: cada lectura
REINICIA la placa (entra y sale del bootloader). No usar a media corrida.

Corre con el Python de WSL; llama a los .exe de Windows.

Uso:  python3 tools/identificar_puertos.py [COM11 COM12 ...]
      (sin argumentos prueba los COM USB que lista arduino-cli)
"""
import json, re, subprocess, sys

CLI = ("/mnt/c/Users/aagus/AppData/Local/Programs/Arduino IDE/resources/app/"
       "lib/backend/resources/arduino-cli.exe")
ESPTOOL = ("/mnt/c/Users/aagus/AppData/Local/Arduino15/packages/esp32/tools/"
           "esptool_py/5.3.1/esptool.exe")

# MAC STA de cada placa de banco (CLAUDE.md, seccion 4)
ROLES = {
    "a4:f0:0f:64:ac:68": "hub",
    "b0:cb:d8:ca:11:f0": "nodo",
}


def puertos_detectados():
    out = subprocess.run([CLI, "board", "list", "--json"],
                         capture_output=True, text=True, timeout=60).stdout
    # Solo los "Serial Port (USB)": los demas COM de esta maquina son
    # Bluetooth/virtuales, no responden a esptool y cada uno cuesta ~30 s
    return [p["port"]["address"] for p in json.loads(out).get("detected_ports", [])
            if p["port"].get("protocol") == "serial"
            and "USB" in p["port"].get("protocol_label", "")]


def leer_mac(puerto):
    try:
        r = subprocess.run([ESPTOOL, "--port", puerto, "read-mac"],
                           capture_output=True, text=True, timeout=30)
    except subprocess.TimeoutExpired:
        return None
    m = re.search(r"MAC:\s*([0-9a-fA-F:]{17})", r.stdout)
    return m.group(1).lower() if m else None


def main():
    puertos = sys.argv[1:] or puertos_detectados()
    for p in puertos:
        mac = leer_mac(p)
        if mac is None:
            print(f"{p:6s}  sin respuesta (no es ESP32, esta ocupado o no entro al bootloader)")
        else:
            print(f"{p:6s}  {mac}  {ROLES.get(mac, 'DESCONOCIDA')}")


if __name__ == "__main__":
    main()
