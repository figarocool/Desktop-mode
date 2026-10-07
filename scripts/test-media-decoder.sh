#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out=${TMPDIR:-/tmp}/desktop-mode-media-decoder-test
cc -std=c99 -Wall -Wextra \
  -I"$root/native/tests/media_mock" -I"$root/native" \
  "$root/native/media.c" "$root/native/tests/media_decoder_test.c" \
  $(pkg-config --cflags --libs libwebp libpng libjpeg) -o "$out"
"$out"
