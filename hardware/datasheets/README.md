# Hojas de datos de la placa de nodo

Una carpeta por hoja del esquemático (la misma división de la guía, `hardware/guia_esquematico/`). Cada PDF es la hoja **del fabricante del código LCSC que se compra**, no la de otro fabricante con el mismo número de parte: un "DW01A" de PUOLOP y uno de H&M no se tratan como idénticos.

Descargadas el 22-nov-2026 desde la página de LCSC de cada código, salvo donde se indica.

## Reglas

1. **Antes de fijar un valor o una conexión en la guía, se cita la hoja de esta carpeta** (archivo y página). Si la pieza cambia de código LCSC, se cambia su hoja aquí en el mismo commit.
2. **Revisar la revisión de la hoja.** La copia que da LCSC puede ser vieja: la del TPS63021 (C202140) es la **revisión E**, que todavía dibujaba VINA unido a VIN; la vigente es la **I** (oct-2019) y es la que está aquí, bajada de TI.
3. Nombre de archivo: `<pieza>_<fabricante>_<código LCSC>.pdf`.
4. Las resistencias y capacitores cerámicos genéricos (C25803, C14663, C15850, etc.) no tienen hoja aquí: son series de catálogo y se revisan por valor, tolerancia, voltaje y encapsulado en la página de LCSC.
5. Una pieza que se usa en varias hojas del esquemático vive en la carpeta donde aparece primero; la columna "También en" lo dice.

## 1_energia (hoja 1 del esquemático)

| Ref | Pieza | Fabricante | LCSC | Archivo | Revisión de la hoja | Verificado | También en |
|---|---|---|---|---|---|---|---|
| Q1, Q7 | AO3401A | AOS | C15127 | `AO3401A_AOS_C15127.pdf` | Rev 3.1, dic-2023 | 16-nov: pinout, VGS ±12 V, RDS(on) | Q5 (hoja 4) |
| F1 | SMD1206P150TF | RUILON | C20802 | `SMD1206P150TF_RUILON_C20802.pdf` | A6, 2026-03-09 | 16-nov: Ihold 1.5 A, Itrip 3 A, Vmax 6 V, reducción por temperatura | |
| D1 | SMAJ5.0A | RUILON | C10758 | `SMAJ5.0A_RUILON_C10758.pdf` | DEC-16 | 16-nov: VRWM 5 V, VC 9.2 V, IR ≤ 800 µA a 5 V | |
| U1 | DW01A | PUOLOP | C351410 | `DW01A_PUOLOP_C351410.pdf` | Rev B, 2016-04-12 | 16-nov: pinout, aplicación típica p. 3 (R1 100 Ω, C1 0.1 µF, **R2 1 kΩ**), TD al aire | |
| Q2 | FS8205A | TECH PUBLIC | C2830320 | `FS8205A_TECHPUBLIC_C2830320.pdf` | sin revisión | 16-nov: pinout, VGS ±10 V, RDS(on) | |
| D6 | B5819W | JSCJ | C8598 | `B5819W_JSCJ_C8598.pdf` | D, mar-2015 | 16-nov: VF ≤ 0.6 V a 1 A, IR | |
| U2 | TPS63021 | TI | C202140 | `TPS63021_TI_SLVS916I_revI_C202140.pdf` (de ti.com) | **Rev I, oct-2019** | 16-nov: pinout, VINA, capacitores, inductor | |
| — | Tarjeta de evaluación TPS63020EVM-487 | TI | — | `TPS63020EVM_TI_SLVU365.pdf` (de ti.com) | SLVU365 | 16-nov: VINA solo con 0.1 µF a GND (fig. 9) | |
| L1 | CKCS5040-1.5uH | CENKER | C5120877 | `CKCS5040-1.5uH_CENKER_C5120877.pdf` | — | 16-nov: Isat 6.3 A, Irms 4.0 A, DCR 18 mΩ | |
| C19 | 10TPE47MAZB (polímero) | Panasonic | C147768 | `10TPE47MAZB_Panasonic_C147768.pdf` (en chino; los números de la tabla se leen igual) | dic-2017 | 23-nov: 47 µF, 10 V (8 V a 105 °C), ESR 35 mΩ, 3.5 × 2.8 × 1.9 mm | |
| C19 (alternativa) | T520B476M010ATE035 (polímero) | KEMET | C141441 | `T520B476M010ATE035_KEMET_C141441.pdf` | 9/5/2017 | 23-nov: 47 µF, 10 V, ESR 35 mΩ; reducción recomendada: 90 % del nominal hasta 10 V | |
| C5 | 6TPB470M (serie TPB) | Panasonic | C76438 | `6TPB470M_Panasonic_C76438.pdf` | hoja de serie, 1 página | 22-nov: 470 µF, 6.3 V, ESR 35 mΩ | |

## 2_usb_carga (hoja 2)

| Ref | Pieza | Fabricante | LCSC | Archivo | Revisión de la hoja | Verificado | También en |
|---|---|---|---|---|---|---|---|
| J3 | TYPE-C-31-M-12 | HRO | C165948 | `TYPE-C-31-M-12_HRO_C165948.pdf` | — | pendiente | |
| U3 | USBLC6-2SC6 | ST | C7519 | `USBLC6-2SC6_ST_C7519.pdf` | — | pendiente | |
| U4 | TP4056 | Top Power | C16581 | `TP4056_TopPower_C16581.pdf` | REV 2.4 | 16-nov: CE, capacitores, LED, TEMP 45–80 %, I = 1100 V / RPROG | |
| U5 | CH340C | WCH | C84681 | `CH340C_WCH_C84681.pdf` | — | pendiente | |
| Q3, Q4 | MMBT3904 | JSCJ | C20526 | `MMBT3904_JSCJ_C20526.pdf` | — | pendiente | Q6 (hoja 4) |
| D2 | KT-0603R (LED rojo) | KENTO | C2286 | `KT-0603R_KENTO_C2286.pdf` | — | pendiente | D4 (hoja 4) |

## 3_esp32 (hoja 3)

| Ref | Pieza | Fabricante | LCSC | Archivo | Revisión de la hoja | Verificado | También en |
|---|---|---|---|---|---|---|---|
| U6 | ESP32-WROOM-32UE-N4 | Espressif | C701344 | `ESP32-WROOM-32E-32UE_Espressif_v2.1_C701344.pdf` (de espressif.com, en inglés; la de LCSC está en chino) | v2.1 | pendiente | |
| SW1, SW2 | TS-1187A-B-A-B | XKB | C318884 | `TS-1187A-B-A-B_XKB_C318884.pdf` | — | pendiente | SW4, SW5 (hoja 4) |
| SW3 | DIP de 2 posiciones | SHOU HAN | C6331180 | `DIP-2P_SHOUHAN_C6331180.pdf` | — | pendiente | |

## 4_sensor_interfaz (hojas 4 y 5)

| Ref | Pieza | Fabricante | LCSC | Archivo | Revisión de la hoja | Verificado | También en |
|---|---|---|---|---|---|---|---|
| J4 | S6B-PH-SM4-TB | JST | C265405 | `S6B-PH-SM4-TB_JST_C265405.pdf` | — | pendiente | |
| D5 | 1N4148W | Shanghai Semtech | C81598 | `1N4148W_SemtechSH_C81598.pdf` | — | pendiente | |
| BZ1 | MLT-5030 | Huaneng | C95297 | `MLT-5030_Huaneng_C95297.pdf` | — | candidato, no elegido: es de 5 mm y la guía pide ≥ 12 mm | |

## Faltan

- Conectores JST-PH de 2 pines (J1, J2, J5): la guía todavía no fija su código LCSC.
- Capacitores de 22 µF, NTC de 100 kΩ en 0603, LED verde, R9 de 3 kΩ: sin código LCSC todavía (guía, sección 10).
- VL53L1X (ST): el sensor va en su módulo, fuera de la placa; st.com bloqueó la descarga automática. Bajarla a mano si se necesita.
