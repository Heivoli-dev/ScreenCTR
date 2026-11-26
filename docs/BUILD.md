# Compilation

## Tests (Linux, macOS ou WSL) — aucune console nécessaire
```bash
sudo apt install gcc g++ python3-pil ffmpeg
bash scripts/run_tests.sh
```

## Partie console
1. Installer [devkitPro](https://devkitpro.org/wiki/Getting_Started) puis `dkp-pacman -S 3ds-dev`.
2. Placer `makerom` ([Project_CTR](https://github.com/3DSGuy/Project_CTR/releases)) dans le `PATH`.
3. `bash scripts/build_console.sh` → `dist/3ds/ScreenCTR.cia` et `ScreenCTR-Launcher.3dsx`.

La police du menu (`console/source/font.h`) est générée par `tools/gen_font.py` depuis
DejaVu Sans Mono ; la version générée est déjà dans le dépôt.

## Application Windows
Prérequis : Visual Studio 2022 (charge de travail C++), CMake ≥ 3.21, Qt 6.5+ MSVC 64 bits,
SDL3 (compilé avec CMake ou paquet officiel).
```powershell
.\scripts\build_windows.ps1 -QtDir C:\Qt\6.7.3\msvc2019_64 -SdlDir C:\libs\SDL3 -FfmpegExe C:\ffmpeg\bin\ffmpeg.exe
```
Résultat : `dist\ScreenCTR\` (ScreenCTR.exe, DLL Qt et SDL3, ffmpeg.exe) — aucun Python
ni installation requise sur la machine de l'utilisateur.

## Intégration continue
`.github/workflows/build.yml` exécute les tests, compile la partie console dans l'image
Docker `devkitpro/devkitarm` et l'application Windows, puis publie les fichiers en artefacts.
