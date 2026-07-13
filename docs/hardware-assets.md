# Hardware, Housings & Documents

Everything that is not firmware: 3D-printed housings, electrical layouts, operating instructions and QA protocols.

Software versioning follows the [Releases](https://github.com/AxelHamburch/ZapBox/releases). Housings (`b…`) and electrical layouts (`e…`) are versioned by **Bitcoin block height** in the same way.

---

## Table of Contents

- [Electrical Layouts](#electrical-layouts)
- [Housing / 3D Models](#housing--3d-models)
- [Operating Instructions](#operating-instructions)
- [Quality Assurance](#quality-assurance)

---

## Electrical Layouts

Complete wiring diagrams — start here when building a ZapBox.

| Variant | Diagram |
|---------|---------|
| **Compact** | [E-Layout-ZapBox-Compact.png](../assets/electric/E-Layout-ZapBox-Compact.png) |
| **Compact-Ext** | [E-Layout-ZapBox-Compact-Ext.png](../assets/electric/E-Layout-ZapBox-Compact-Ext.png) |
| **Duo** | [E-Layout-ZapBox-Duo.png](../assets/electric/E-Layout-ZapBox-Duo.png) |
| **Quattro** | [E-Layout-ZapBox-Quattro.png](../assets/electric/E-Layout-ZapBox-Quattro.png) |
| **ZapOMat** | [E-Layout-ZapBox-ZapOMat.png](../assets/electric/E-Layout-ZapBox-ZapOMat.png) |
| **Servo** | [E-Layout-ZapBox-Servo.png](../assets/electric/E-Layout-ZapBox-Servo.png) |
| **Touch 3.5" — ONE** | [E-Layout-ZapBox-Touch3.5-ONE.webp](../assets/electric/E-Layout-ZapBox-Touch3.5-ONE.webp) |
| **Touch 3.5" — FOUR** | [E-Layout-ZapBox-Touch3.5-FOUR.webp](../assets/electric/E-Layout-ZapBox-Touch3.5-FOUR.webp) |
| **Headless** | [E-Layout-ZapBox-Headless.png](../assets/electric/E-Layout-ZapBox-Headless.png) |
| **Headless Servo** | [E-Layout-ZapBox-Headless-Servo.png](../assets/electric/E-Layout-ZapBox-Headless-Servo.png) |
| **ESP32-C3-21-1** | [E-Layout-ZapBox-esp32-c3-21-1.png](../assets/electric/E-Layout-ZapBox-esp32-c3-21-1.png) |

**Miscellaneous:** [ZapSave](../assets/electric/E-Layout-ZapBox-ZapSave.png) · [USB-Power-Hub](../assets/electric/E-Layout-ZapBox-USB-Power-Hub.png)

### Version history (Inkscape)

| Version | Type | Comment |
|---------|------|---------|
| e926834 | Compact | Prototype |
| e928304 | Compact | Prototype 2 |
| e928556 | Compact | Sample device |
| e931557 | Duo | First Duo |
| e932547 | Quattro | First Quattro |
| e932714 | Duo | Duo update |
| e935776 | Headless | First Headless |
| e932547 | Quattro | Add update button cable & IR light barrier |
| e937540 | Duo | Duo update |
| e937544 | USB-Power-Hub | First USB-Power-Hub |
| e938714 | ZapOMat | First ZapOMat design |
| e938889 | Headless | Update Headless with ZapBox picture |
| e938897 | Compact | Update Compact with ZapBox picture |
| e939705 | ZapOMat | ZapOMat No.1 |
| e940540 | Headless | Update |
| e943674 | Servo | Start the Servo story |
| e944644 | Headless Servo | First powerful Headless Servo |
| e945370 | ZapOMat | Update with wide-range voltage input |
| e946465 | Servo | Update with wide-range voltage input and NFC plug |
| e947689 | Compact-Ext | Special version with external guided switching contact |
| e948960 | Headless Servo | Add NFC Tag 2 module |
| e948971 | Headless | Add NFC Tag 2 module |
| e949393 | esp32-c3-21-1 | Add special Hans Wurst version |
| e949674 | Servo | Add NFC Tag 2 module & minor redesign |
| e950677 | Compact | Add NFC Tag 2 module |
| e950939 | Touch3.5 | ESP32-S Touch3.5 (JC3248W535C) — prototype |
| e955640 | ZapSave | ZapSave — sample connection |
| e957556 | Touch3.5-FOUR | ZapBox Touch 3.5 with 4 channels |
| e957575 | Touch3.5-ONE | ZapBox Touch 3.5 with 1 channel |

→ All versions: [assets/electric/](https://github.com/AxelHamburch/ZapBox/tree/main/assets/electric)

---

## Housing / 3D Models

Designed in **FreeCAD**, exported as `.3mf` for printing.

| Version | Type | Comment |
|---------|------|---------|
| b926837 | Compact | Prototype, uses e926834 |
| b928260 | Compact | Prototype 2, uses e928304 |
| b928555 | Compact | Sample device, uses e928556 |
| b930595 | Compact | Optimization, separate label |
| b931760 | Duo | Prototype Duo with two front panels, 90° and 35° |
| b932506 | Compact | Adapter system, 90° front, USB-C position changed |
| b932595 | Duo & Quattro | Prototype Quattro and Duo update, 90° and 35° front |
| b932788 | Illuminated Sign | Prototype LED sign for demonstration and testing |
| b935750 | Headless | Prototype Headless — ZapBox without display |
| b937454 | USB-Power-Hub | Prototype USB-Power-Hub — voltage distribution only |
| b939002 | Compact | Compact 35° with NFC cap |
| b939704 | ZapOMat | ZapOMat No.1 |
| b940298 | Duo | Update & NFC lid |
| b943400 | Headless | Headless with NFC |
| b943614 | Servo | The first one with servo control |
| b944177 | Headless | Mounting plate with snap-fit connection |
| b944666 | Headless Servo | First powerful Headless Servo |
| b945188 | ZapOMat | Mounting plate and wide-range voltage input |
| b946303 | Servo | Mounting plate and tweaks |
| b946400 | Compact | Mounting plate, 90° NFC and Ext. option |
| b948772 | Headless Servo | Add NFC Tag 2 module |
| b948929 | Headless | Add NFC Tag 2 module |
| b949639 | Servo | Add NFC Tag 2 module |
| b950530 | Compact | Add NFC Tag 2 module |
| b950711 | Touch 3.5 | ESP32-S Touch3.5 (JC3248W535C) No.1 — prototype |
| b955706 | ZapSave | ZapSave — sample box |
| b956540 | Touch3.5-FOUR | ZapBox Touch 3.5 with 4 channels — incl. battery holder |
| b957183 | Touch3.5-ONE | ZapBox Touch 3.5 with 1 channel — incl. battery holder |

→ All versions: [assets/housing/](https://github.com/AxelHamburch/ZapBox/tree/main/assets/housing)

---

## Operating Instructions

User manuals in Markdown (for reading online) and PDF (for printing).

| | Markdown | PDF |
|---|----------|-----|
| **Compact** | [de](../assets/operating-instructions/Compact-oi-de.md) / [en](../assets/operating-instructions/Compact-oi-en.md) | [de](../assets/operating-instructions/Compact-oi-de.pdf) / [en](../assets/operating-instructions/Compact-oi-en.pdf) |
| **Duo** | [de](../assets/operating-instructions/Duo-oi-de.md) / [en](../assets/operating-instructions/Duo-oi-en.md) | [de](../assets/operating-instructions/Duo-oi-de.pdf) / [en](../assets/operating-instructions/Duo-oi-en.pdf) |
| **Quattro** | [de](../assets/operating-instructions/Quattro-oi-de.md) / [en](../assets/operating-instructions/Quattro-oi-en.md) | [de](../assets/operating-instructions/Quattro-oi-de.pdf) / [en](../assets/operating-instructions/Quattro-oi-en.pdf) |
| **ZapOMat** | [de](../assets/operating-instructions/ZapOMat-oi-de.md) / [en](../assets/operating-instructions/ZapOMat-oi-en.md) | [de](../assets/operating-instructions/ZapOMat-oi-de.pdf) / [en](../assets/operating-instructions/ZapOMat-oi-en.pdf) |
| **Servo** | [de](../assets/operating-instructions/Servo-oi-de.md) / [en](../assets/operating-instructions/Servo-oi-en.md) | [de](../assets/operating-instructions/Servo-oi-de.pdf) / [en](../assets/operating-instructions/Servo-oi-en.pdf) |
| **Headless** | [de](../assets/operating-instructions/Headless-oi-de.md) / [en](../assets/operating-instructions/Headless-oi-en.md) | [de](../assets/operating-instructions/Headless-oi-de.pdf) / [en](../assets/operating-instructions/Headless-oi-en.pdf) |
| **Headless Servo** | [de](../assets/operating-instructions/Headless-Servo-oi-de.md) / [en](../assets/operating-instructions/Headless-Servo-oi-en.md) | [de](../assets/operating-instructions/Headless-Servo-oi-de.pdf) / [en](../assets/operating-instructions/Headless-Servo-oi-en.pdf) |

→ Archive and development: [assets/operating-instructions/](https://github.com/AxelHamburch/ZapBox/tree/main/assets/operating-instructions)

---

## Quality Assurance

Templates for QA protocols to ensure ZapBox build quality.

- [Initial Review (German)](../assets/QA/initial-review-de.md) — [PDF](../assets/QA/initial-review-de.pdf)
- [Initial Review (English)](../assets/QA/initial-review-en.md) — [PDF](../assets/QA/initial-review-en.pdf)

→ Archive and development: [assets/QA/](https://github.com/AxelHamburch/ZapBox/tree/main/assets/QA)
