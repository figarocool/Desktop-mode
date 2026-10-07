#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out=${TMPDIR:-/tmp}/desktop-mode-wasm-runtime-test
bash "$root/scripts/build-wasm-apps.sh"
cc -std=c99 -w -Dd_m3MaxLinearMemoryPages=1024 \
  -I"$root/native" -I"$root/native/vendor/wasm3" \
  "$root/native/tests/wasm_app_runtime_test.c" \
  "$root/native/wasm_app_runtime.c" \
  "$root/native/vendor/browser-engine/duktape/src/duktape.c" \
  "$root/native/vendor/wasm3/m3_api_libc.c" \
  "$root/native/vendor/wasm3/m3_api_tracer.c" \
  "$root/native/vendor/wasm3/m3_bind.c" \
  "$root/native/vendor/wasm3/m3_code.c" \
  "$root/native/vendor/wasm3/m3_compile.c" \
  "$root/native/vendor/wasm3/m3_core.c" \
  "$root/native/vendor/wasm3/m3_emit.c" \
  "$root/native/vendor/wasm3/m3_env.c" \
  "$root/native/vendor/wasm3/m3_exec.c" \
  "$root/native/vendor/wasm3/m3_function.c" \
  "$root/native/vendor/wasm3/m3_info.c" \
  "$root/native/vendor/wasm3/m3_module.c" \
  "$root/native/vendor/wasm3/m3_parse.c" -lm -lcurl -o "$out"
if [ -n "${DM_SOFTSHOP_REPLAY_DIR:-}" ]; then
  "$out" "$root/native/app-sdk/examples/wasm-browser.wasm" browser softshop "$DM_SOFTSHOP_REPLAY_DIR"
  exit 0
fi
"$out" "$root/native/app-sdk/examples/wasm-counter.wasm" counter
"$out" "$root/native/app-sdk/examples/wasm-calculator.wasm" calculator
"$out" "$root/native/app-sdk/examples/wasm-taskmanager.wasm" taskmanager
"$out" "$root/native/app-sdk/examples/wasm-images.wasm" images
"$out" "$root/native/app-sdk/examples/wasm-notepad.wasm" notepad
"$out" "$root/native/app-sdk/examples/wasm-paint.wasm" paint
"$out" "$root/native/app-sdk/examples/wasm-console.wasm" console
"$out" "$root/native/app-sdk/examples/wasm-browser.wasm" browser
"$out" "$root/native/app-sdk/examples/wasm-pdf.wasm" pdf
"$out" "$root/native/app-sdk/examples/wasm-network.wasm" network
"$out" "$root/native/app-sdk/examples/wasm-solitaire.wasm" solitaire
"$out" "$root/native/app-sdk/examples/wasm-minesweeper.wasm" minesweeper
"$out" "$root/native/app-sdk/examples/wasm-media.wasm" media
echo "PASS: bundled WASM apps exercise their host APIs"
