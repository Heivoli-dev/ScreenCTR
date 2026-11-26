#!/usr/bin/env bash
# ScreenCTR — build the 3DS side (sysmodule CIA + launcher 3DSX).
# Prerequisites: devkitPro with the 3ds-dev group (devkitARM, libctru), makerom in PATH.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${DEVKITARM:?Installez devkitPro (pacman -S 3ds-dev) puis ouvrez un nouveau terminal}"
command -v makerom >/dev/null || { echo "makerom introuvable (github.com/3DSGuy/Project_CTR/releases)"; exit 1; }
python3 "$ROOT/tools/gen_font.py" "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf" "$ROOT/console/source/font.h" 2>/dev/null || true
make -C "$ROOT/console"
make -C "$ROOT/console" launcher
mkdir -p "$ROOT/dist/3ds"
cp "$ROOT/console/ScreenCTR.cia" "$ROOT/console/launcher/ScreenCTR-Launcher.3dsx" "$ROOT/dist/3ds/"
echo "Fichiers console dans $ROOT/dist/3ds"
