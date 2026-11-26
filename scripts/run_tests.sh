#!/usr/bin/env bash
# ScreenCTR — runs every test that does not need a 3DS, Qt or Windows.
# Requirements: gcc, g++, python3 + Pillow, ffmpeg (optional, recorder test).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-$(mktemp -d)}"
INC="-I$ROOT/protocol/include -I$ROOT/common/jpeg -I$ROOT/common/pixfmt -I$ROOT/common/config"
COMMON_SRC="$ROOT/protocol/src/sc_protocol.c $ROOT/common/jpeg/sc_jpeg.c $ROOT/common/pixfmt/sc_pixfmt.c $ROOT/common/config/sc_config.c"
pass=0; fail=0
step() { echo; echo "== $1"; }
ok() { pass=$((pass+1)); }
ko() { fail=$((fail+1)); echo "!! $1"; }

step "Bibliothèque commune (protocole, JPEG, pixels, configuration)"
gcc -std=c99 -O2 -Wall -Wextra -Werror $INC "$ROOT/tests/test_common.c" $COMMON_SRC -o "$OUT/test_common"
"$OUT/test_common" "$OUT" && ok || ko "test_common"

step "Encodeur JPEG contre un décodeur indépendant (Pillow/libjpeg)"
python3 "$ROOT/tests/check_jpeg.py" "$OUT" && ok || ko "check_jpeg"

step "Cœur de l'application PC (dispositions, tactile, manettes, statistiques)"
gcc -std=c99 -O2 -c -I"$ROOT/protocol/include" "$ROOT/protocol/src/sc_protocol.c" -o "$OUT/proto.o"
g++ -std=c++17 -O1 -Wall -Wextra -Werror -I"$ROOT/pc/core" -I"$ROOT/protocol/include" \
    "$ROOT/tests/test_pc_core.cpp" "$ROOT"/pc/core/*.cpp "$OUT/proto.o" -o "$OUT/test_pc_core"
"$OUT/test_pc_core" && ok || ko "test_pc_core"

step "Vérification syntaxique du code console (en-têtes libctru de substitution — PAS une compilation 3DS)"
for f in "$ROOT"/console/source/*.c "$ROOT"/console/launcher/source/main.c; do
    gcc -std=gnu11 -fsyntax-only -Wno-format -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast \
        -I"$ROOT/tests/stubs" -I"$ROOT/console/source" $INC "$f" || { ko "syntax $f"; continue; }
done
ok

step "Fausse console + test de bout en bout (sans perte, puis 5 % de perte)"
gcc -std=c99 -O2 -Wall -Wextra $INC "$ROOT/tools/mock_console/mock_console.c" $COMMON_SRC -o "$OUT/mock_console"
python3 "$ROOT/tests/e2e_test.py" "$OUT/mock_console" && ok || ko "e2e 0%"
python3 "$ROOT/tests/e2e_test.py" "$OUT/mock_console" --loss 0.05 && ok || ko "e2e 5%"

if command -v ffmpeg >/dev/null; then
    step "Commande FFmpeg de l'enregistreur (même arguments que Recorder.cpp)"
    python3 "$ROOT/tests/recorder_pipe_test.py" "$OUT/rec.mp4" && ok || ko "recorder"
fi

echo; echo "Résultat : $pass étape(s) réussie(s), $fail échec(s). Fichiers : $OUT"
[ "$fail" -eq 0 ]
