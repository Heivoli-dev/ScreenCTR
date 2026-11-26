# Crédits

**ScreenCTR** — Heivoli Network — *Two screens. One connection.*
Licence : GPL-3.0-or-later (voir `LICENSE`).

ScreenCTR est écrit de zéro. Les projets suivants ont été étudiés pour comprendre les
mécanismes du système 3DS. Seule exception : `console/source/csvc.s` reprend (réduits)
les wrappers d'appels système étendus de Luma3DS (`rosalina/source/csvc.s`, licence
permissive de type zlib, notice conservée).

- **Luma3DS / Rosalina** (LumaTeam, GPLv3) — protocole et fonctionnement
  d'InputRedirection (port 4950, valeurs neutres), SVC étendus (`svcControlProcess`,
  `svcMapProcessMemoryEx`), modèle d'initialisation d'un sysmodule.
- **HzMod** (Sono) et **ChirunoMod** (ChainSwordCS, GPLv3) — capture via
  `GSPGPU_ImportDisplayCaptureInfo` + `svcStartInterProcessDma`, utilisation du cœur 2.
- **NTR CFW** — référence historique de la diffusion sans fil 3DS.
- **3dbrew.org** — documentation des services et formats.

Bibliothèques et outils :
- **libctru**, **devkitARM**, **makerom** (devkitPro, Project_CTR).
- **Qt 6** (LGPLv3), **SDL3** (zlib), **FFmpeg** (exécutable séparé, LGPL/GPL selon la compilation).
- **DejaVu Sans Mono** (licence Bitstream Vera) — police bitmap du menu console, générée par `tools/gen_font.py`.
- Tables JPEG standard de la recommandation ITU-T T.81 (annexe K).
