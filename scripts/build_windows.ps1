# ScreenCTR — local Windows build
# Prerequisites: Visual Studio 2022 (C++), CMake >= 3.21, Qt 6.5+ (MSVC 64-bit), SDL3 (built or installed)
# Usage:  .\scripts\build_windows.ps1 -QtDir C:\Qt\6.7.3\msvc2019_64 -SdlDir C:\libs\SDL3
param(
    [Parameter(Mandatory = $true)][string]$QtDir,
    [Parameter(Mandatory = $true)][string]$SdlDir,
    [string]$FfmpegExe = ""
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "pc\build"
$dist = Join-Path $root "dist\ScreenCTR"

cmake -S (Join-Path $root "pc") -B $build -DCMAKE_PREFIX_PATH="$QtDir;$SdlDir" -DCMAKE_INSTALL_PREFIX="$dist"
cmake --build $build --config Release
ctest --test-dir $build -C Release --output-on-failure
cmake --install $build --config Release

if ($FfmpegExe -ne "") { Copy-Item $FfmpegExe (Join-Path $dist "ffmpeg.exe") }
Copy-Item (Join-Path $root "LICENSE") $dist
Copy-Item (Join-Path $root "CREDITS.md") $dist
Write-Host "ScreenCTR.exe et ses dépendances sont dans $dist"
