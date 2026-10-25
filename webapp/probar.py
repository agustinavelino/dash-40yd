#!/usr/bin/env python3
"""
Prueba de la pagina del coach con el hub simulado (sim.js) - DASH Timing System

Recorre el flujo completo en un Chromium sin ventana, a 390 px de ancho:
elegir atleta -> armar -> corrida -> puerta sin senal -> conexion caida ->
Sesion -> alta/edicion de atleta -> distancias -> CSV. Comprueba lo que se
puede comprobar sin placas y deja una captura por pantalla.

Requisitos: el venv del repo con playwright
  .venv/bin/pip install playwright && .venv/bin/python -m playwright install chromium

Uso:  .venv/bin/python webapp/probar.py [carpeta_de_capturas]
"""
import gzip, pathlib, re, sys
from playwright.sync_api import sync_playwright

AQUI = pathlib.Path(__file__).resolve().parent
PAGINA = AQUI / "index.html"
SALIDA = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else AQUI / "capturas"
SALIDA.mkdir(exist_ok=True)
fallas = []


def revisar(cond, msg):
    print(("  ok    " if cond else "  FALLA ") + msg)
    if not cond:
        fallas.append(msg)


# --- estatico: tamano y recursos externos (REQ-NF-08, Maestro 13.3) ---
html = PAGINA.read_bytes()
gz = len(gzip.compress(html, 9))
print(f"index.html: {len(html) / 1024:.1f} KB, {gz / 1024:.1f} KB con gzip")
revisar(len(html) <= 30 * 1024, "pesa <= 30 KB")
externos = re.findall(rb'(?:src|href)\s*=\s*["\']?(?:https?:)?//|@import|<link', html)
revisar(not externos, "sin recursos externos")

with sync_playwright() as p:
    nav = p.chromium.launch()
    pg = nav.new_page(viewport={"width": 390, "height": 844}, accept_downloads=True)
    errores = []
    pg.on("pageerror", lambda e: errores.append(str(e)))
    pg.on("console", lambda m: errores.append(m.text) if m.type == "error" else None)
    foto = lambda n: pg.screenshot(path=SALIDA / f"{n}.png", full_page=True)
    tab = lambda n: (pg.click(f'nav button[data-ir="{n}"]'), pg.wait_for_timeout(200))

    pg.goto(PAGINA.as_uri() + "?sim=1&rapido=1")
    pg.evaluate("localStorage.clear()")          # roster y distancias de fabrica del simulador
    pg.reload(); pg.wait_for_timeout(700)
    foto("01_vivo_inicio")
    revisar(pg.is_disabled("#armar"), "no se puede armar sin atleta")

    # tres corridas de dos atletas
    for atl in ("1", "1", "3"):
        pg.select_option("#sel", atl); pg.click("#armar"); pg.wait_for_timeout(1400)
    foto("02_vivo_resultado")
    retardo = pg.evaluate("window.ultimoDibujo - window.sim.ultimoEnvio")
    revisar(retardo < 2000, f"resultado en pantalla {retardo:.1f} ms despues de llegar (REQ-F-05: <= 2 s)")
    meta = pg.evaluate("S.ultima.splits_ms[2]")       # el simulador pierde una puerta al azar el 5 % de las veces
    revisar(re.match(r"^\d\.\d\ds$", pg.inner_text("#grande")) is not None if meta is not None
            else pg.inner_text("#grande") == "—", "tiempo final a centesimas (o guion si la meta no detecto)")

    pg.select_option("#sel", "2"); pg.click("#armar"); pg.wait_for_timeout(200)
    foto("03_vivo_armado")
    revisar(pg.get_attribute("#armar", "aria-pressed") == "true", "boton en estado armado")
    pg.wait_for_timeout(1300)

    pg.evaluate("sim.puerta(2, false)")
    pg.select_option("#sel", "4"); pg.click("#armar"); pg.wait_for_timeout(1400)
    foto("04_vivo_puerta_sin_senal")
    revisar("—" in pg.inner_text("#cancha"), "puerta sin deteccion se muestra como guion, sin inventar el dato")
    pg.evaluate("sim.puerta(2, true)")

    pg.evaluate("sim.caer(2500)"); pg.wait_for_timeout(300)
    foto("05_vivo_sin_conexion")
    revisar(pg.is_visible("#conexion"), "banda de conexion caida visible")
    pg.wait_for_timeout(3500)
    revisar(not pg.is_visible("#conexion"), "reconecta solo")

    tab("sesion")
    filas = pg.locator("#filas tr").count()
    revisar(filas == 5, f"Sesion muestra las 5 corridas ({filas})")
    pg.click('[data-orden="tiempo"]'); pg.wait_for_timeout(100)
    foto("06_sesion_mas_rapidos")
    pg.select_option("#filtro", "1"); pg.wait_for_timeout(100)
    revisar(pg.locator("#filas tr").count() == 2, "filtro por atleta")
    pg.select_option("#filtro", "")

    tab("atletas")
    pg.fill("#nNum", "22"); pg.fill("#nNombre", "Repetido"); pg.click("#nuevo button"); pg.wait_for_timeout(200)
    revisar("ya es de" in pg.inner_text("#avisoRoster"), "rechaza numero de dorsal repetido")
    pg.fill("#nNum", "15"); pg.fill("#nNombre", "Rodrigo Méndez, Jr."); pg.click("#nuevo button"); pg.wait_for_timeout(300)
    revisar("Rodrigo" in pg.inner_text("#roster"), "alta de atleta")
    pg.click('button[aria-label="Editar a Mateo Salinas"]'); pg.wait_for_timeout(100)
    foto("07_atletas_editando")
    pg.click(".roster .peligro"); pg.click(".roster .peligro"); pg.wait_for_timeout(300)
    revisar("Mateo" not in pg.inner_text("#roster"), "baja de atleta con doble confirmacion")
    foto("08_atletas")

    tab("ajustes")
    pg.fill("#d2", "5"); pg.click("#distancias button"); pg.wait_for_timeout(200)
    revisar("crecientes" in pg.inner_text("#avisoDist"), "rechaza distancias no crecientes")
    pg.fill("#d2", "25"); pg.click("#distancias button"); pg.wait_for_timeout(300)
    revisar("25" in pg.inner_text("#avisoDist"), "guarda distancias nuevas")
    with pg.expect_download() as d:
        pg.click("#csv")
    pg.wait_for_timeout(200)
    foto("09_ajustes")
    csv = pg.evaluate("window.ultimoCsv").splitlines()
    print("  CSV:", *csv[:3], sep="\n    ")
    revisar(csv[0] == "corrida,atleta,num,t1_s,t2_s,t3_s,yd1,yd2,yd3,fecha", "encabezado del CSV segun PROTOCOLO.md")
    revisar(len(csv) == 6, f"CSV con 5 corridas ({len(csv) - 1})")
    revisar(all(re.match(r"^\d+,", l) for l in csv[1:]), "una fila por corrida")
    revisar(all(re.search(r",(\d\.\d\d)?,(\d\.\d\d)?,(\d\.\d\d)?,10,20,40,", l) for l in csv[1:]),
            "tiempos a centesimas y distancias con las que se midio cada corrida")
    revisar(d.value.suggested_filename.startswith("dash_"), "descarga con nombre dash_<fecha>.csv")

    tab("vivo")
    pg.select_option("#sel", "1"); pg.click("#armar"); pg.wait_for_timeout(1400)
    tab("sesion")
    foto("10_sesion_distancia_movida")
    revisar("25" in pg.text_content("#h2") and pg.locator("#filas small.otra").count() == 5,
            "la corrida nueva usa 25 yd y las 5 anteriores muestran que se midieron a 20 (REQ-F-06)")

    revisar(not errores, "sin errores de JavaScript" + ("" if not errores else ": " + "; ".join(errores)))
    nav.close()

print(f"\n{'TODO OK' if not fallas else str(len(fallas)) + ' FALLAS'} · capturas en {SALIDA}")
sys.exit(1 if fallas else 0)
