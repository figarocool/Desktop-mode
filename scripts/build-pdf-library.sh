#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
platform=${1:-linux}
case "$platform" in
 linux) compiler=cc; archiver=ar; flags="-DDESKTOP_PREVIEW $(pkg-config --cflags freetype2)" ;;
 vita) compiler="${VITASDK:-/usr/local/vitasdk}/bin/arm-vita-eabi-gcc"; archiver="${VITASDK:-/usr/local/vitasdk}/bin/arm-vita-eabi-ar"; flags="-I${VITASDK:-/usr/local/vitasdk}/arm-vita-eabi/include/freetype2" ;;
 *) echo "Usage: $0 linux|vita" >&2; exit 1 ;;
esac
output="native/build/pdf-library-$platform"
mkdir -p "$output"
$compiler -std=gnu11 -Wall -Wextra -Werror -O2 $flags -c native/pdf/desktop_pdf.c -o "$output/desktop_pdf.o"
$archiver rcs "$output/libdesktop_pdf.a" "$output/desktop_pdf.o"
cp native/apps/pdf_engine.h "$output/pdf_engine.h"
printf '%s\n' "$output/libdesktop_pdf.a"
