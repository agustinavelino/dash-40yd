#!/usr/bin/env python3
"""Convierte guia_esquematico.html en guia_esquematico.pdf con el Chromium de Playwright.
Uso: .venv/bin/python hardware/guia_esquematico/a_pdf.py"""
import pathlib
from playwright.sync_api import sync_playwright

aqui = pathlib.Path(__file__).resolve().parent
with sync_playwright() as p:
    nav = p.chromium.launch()
    pg = nav.new_page()
    pg.goto((aqui / "guia_esquematico.html").as_uri())
    pg.pdf(path=str(aqui / "guia_esquematico.pdf"), format="A4", print_background=True, prefer_css_page_size=True,
           display_header_footer=True, header_template="<span></span>",
           footer_template='<div style="font-size:8pt;width:100%;text-align:center;color:#777">'
                           'DASH · Guía del esquemático · <span class="pageNumber"></span> / <span class="totalPages"></span></div>')
    nav.close()
print("ok", aqui / "guia_esquematico.pdf")
