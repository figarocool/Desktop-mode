#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
sdk=${VITASDK:-}
if [ -z "$sdk" ] && [ -f "$project_root/native/build/CMakeCache.txt" ]; then
    sdk=$(sed -n 's/^VITASDK:PATH=//p' "$project_root/native/build/CMakeCache.txt" | head -n 1)
fi
if [ -z "$sdk" ]; then
    for candidate in /usr/local/vitasdk /opt/vitasdk; do
        if [ -d "$candidate/arm-vita-eabi" ]; then sdk=$candidate; break; fi
    done
fi
if [ -z "$sdk" ] || [ ! -d "$sdk/arm-vita-eabi" ]; then
    echo "Non trovo il VitaSDK. Imposta VITASDK o configura prima native/build." >&2
    exit 1
fi
sdk=${sdk%/}
export VITASDK=$sdk

vdpm_bin=$(command -v vdpm 2>/dev/null || true)
if [ -z "$vdpm_bin" ] && [ -x "$sdk/bin/vdpm" ]; then vdpm_bin=$sdk/bin/vdpm; fi
if [ -z "$vdpm_bin" ]; then
    echo "vdpm non trovato. Aggiorna VitaSDK con il bootstrap ufficiale e riprova." >&2
    echo "Documentazione: https://vitasdk.org/" >&2
    exit 1
fi

# The installed March 2025 SDK uses vdpm's legacy syntax; newer releases use
# `vdpm install`. Install transitive codec packages first for legacy vdpm.
if "$vdpm_bin" -h 2>&1 | grep -q 'Usage: ./vdpm'; then
    "$vdpm_bin" libogg flac lame libvorbis mpg123 opus libsndfile ffmpeg fluidsynth-lite
else
    "$vdpm_bin" install libsndfile ffmpeg fluidsynth-lite
fi

if [ ! -f "$sdk/arm-vita-eabi/include/sndfile.h" ] || [ ! -f "$sdk/arm-vita-eabi/lib/libsndfile.a" ]; then
    echo "vdpm non ha installato libsndfile: controlla la connessione o i log del package manager." >&2
    exit 1
fi

echo "libsndfile è installata nel VitaSDK. Ricompila: cmake --build native/build --target desktop-mode.vpk-vpk -j2"
