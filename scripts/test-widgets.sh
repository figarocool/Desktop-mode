#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
for app_source in "$root"/native/app-sdk/examples/wasm-*.c; do
  rg -q '#include "../dm_widgets.h"' "$app_source"
  rg -q 'dmw_' "$app_source"
done
out=${TMPDIR:-/tmp}/desktop-mode-widgets-api-test
cc -std=c99 -Wall -Wextra -Wno-unused-function "$root/native/tests/widgets_api_test.c" -o "$out"
"$out"
clang --target=wasm32 -std=c99 -O2 -nostdlib -fno-builtin \
  -c -o "${TMPDIR:-/tmp}/desktop-mode-widgets-wasm.o" \
  "$root/native/tests/widgets_wasm_compile.c"
echo "PASS: native and WASM widget APIs compile and basic interactions work"
