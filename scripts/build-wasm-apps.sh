#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
wasm_clang=${WASM_CLANG:-clang}
wasm_linker=${WASM_LINKER:-wasm-ld-15}
examples=$root/native/app-sdk/examples
engine=$root/native/vendor/browser-engine
browser_build=${TMPDIR:-/tmp}/desktop-mode-browser-wasm

build_app() {
    app=$1
    shift
    object=${TMPDIR:-/tmp}/desktop-mode-${app}.o
    "$wasm_clang" --target=wasm32 -std=c99 -O2 -nostdlib -fno-builtin \
        -c -o "$object" "$examples/wasm-${app}.c"
    export_args=
    while [ "$#" -gt 0 ]; do
        export_name=$1
        shift
        export_args="$export_args --export=$export_name"
    done
    # These export names are fixed by the local SDK source, not user input.
    # shellcheck disable=SC2086
    "$wasm_linker" --no-entry --allow-undefined $export_args \
        -o "$examples/wasm-${app}.wasm" "$object"
}

build_browser() {
    emscripten_root=$(em-config EMSCRIPTEN_ROOT)
    sysroot=$emscripten_root/cache/sysroot/lib/wasm32-emscripten
    mkdir -p "$browser_build"
    cmake -S "$engine/lexbor" -B "$browser_build/lexbor" \
        -DCMAKE_TOOLCHAIN_FILE="$emscripten_root/cmake/Modules/Platform/Emscripten.cmake" \
        -DLEXBOR_BUILD_TESTS=OFF -DLEXBOR_BUILD_TESTS_CPP=OFF \
        -DLEXBOR_BUILD_EXAMPLES=OFF -DLEXBOR_BUILD_UTILS=OFF \
        -DLEXBOR_BUILD_SHARED=OFF -DLEXBOR_BUILD_STATIC=ON \
        -DLEXBOR_BUILD_SEPARATELY=OFF -DCMAKE_BUILD_TYPE=Release >/dev/null
    cmake --build "$browser_build/lexbor" -j2 >/dev/null
    emcc -O2 -D__wasm__ -I"$engine/lexbor/source" -c \
        -o "$browser_build/browser.o" "$examples/wasm-browser.c"
    "$wasm_linker" --no-entry --allow-undefined --export=memory \
        --export=dm_app_abi_version --export=dm_app_text_buffer \
        --export=dm_app_init --export=dm_app_draw --export=dm_app_click \
        --export=dm_app_text --export=dm_app_key --export=dm_app_menu \
        --export=dm_app_tick --export=dm_app_close \
        --initial-memory=25165824 --max-memory=67108864 \
        -o "$examples/wasm-browser.wasm" "$browser_build/browser.o" \
        "$browser_build/lexbor/liblexbor_static.a" \
        "$sysroot/libc.a" "$sysroot/libdlmalloc.a" "$sysroot/libcompiler_rt.a"
}

build_app counter dm_app_abi_version dm_app_init dm_app_draw dm_app_click dm_app_menu dm_app_close
build_app calculator dm_app_abi_version dm_app_text_buffer dm_app_text dm_app_key dm_app_init dm_app_draw dm_app_click dm_app_menu dm_app_close
build_app taskmanager dm_app_abi_version dm_app_init dm_app_draw dm_app_click dm_app_menu dm_app_close
build_app images dm_app_abi_version dm_app_argument_buffer dm_app_file_buffer dm_app_file_result dm_app_init dm_app_draw dm_app_click dm_app_menu dm_app_close
build_app notepad dm_app_abi_version dm_app_argument_buffer dm_app_text_buffer dm_app_file_buffer dm_app_file_result dm_app_init dm_app_draw dm_app_click dm_app_text dm_app_key dm_app_menu dm_app_event dm_app_tick dm_app_dirty dm_app_discard dm_app_close
build_app paint dm_app_abi_version dm_app_file_buffer dm_app_file_result dm_app_text_buffer dm_app_text dm_app_init dm_app_draw dm_app_click dm_app_key dm_app_menu dm_app_tick dm_app_dirty dm_app_discard dm_app_close
build_app console dm_app_abi_version dm_app_text_buffer dm_app_event dm_app_init dm_app_draw dm_app_click dm_app_text dm_app_key dm_app_menu dm_app_close
build_browser
build_app pdf dm_app_abi_version dm_app_argument_buffer dm_app_text_buffer dm_app_file_buffer dm_app_file_result dm_app_init dm_app_draw dm_app_click dm_app_text dm_app_key dm_app_menu dm_app_tick dm_app_close
build_app network dm_app_abi_version dm_app_init dm_app_draw dm_app_click dm_app_key dm_app_menu dm_app_tick dm_app_close
build_app solitaire dm_app_abi_version dm_app_init dm_app_draw dm_app_click dm_app_key dm_app_menu dm_app_tick dm_app_close
build_app minesweeper dm_app_abi_version dm_app_init dm_app_draw dm_app_click dm_app_key dm_app_menu dm_app_tick dm_app_close
build_app media dm_app_abi_version dm_app_argument_buffer dm_app_file_buffer dm_app_file_result dm_app_init dm_app_draw dm_app_click dm_app_key dm_app_menu dm_app_tick dm_app_close

echo "Built thirteen Desktop Mode WASM apps."
