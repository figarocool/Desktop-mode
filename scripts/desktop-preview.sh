#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p native/build/apps native/build/test-modules
bash scripts/build-wasm-apps.sh
for app_name in notepad counter browser calculator taskmanager paint images console pdf network solitaire minesweeper media; do
 python3 native/app-sdk/package-app.py "native/app-sdk/examples/wasm-$app_name.wasm" wasm "native/build/apps/$app_name.dmapp"
done
cmake -S native/vendor/libsmb2 -B native/build/smb2-linux -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DENABLE_LIBKRB5=OFF -DENABLE_GSSAPI=OFF -DENABLE_LIBDCERPC=OFF -DENABLE_EXAMPLES=OFF -DENABLE_UTILS=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5 >native/build/smb2-config.log
cmake --build native/build/smb2-linux -j2 >native/build/smb2-build.log
if [ -n "${DESKTOP_SELF_TEST:-}" ]; then
 cc -DDM_PLUGIN_LINUX -DDM_TRACK_MEMORY -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=free -std=gnu11 -Wall -Wextra -O2 -fPIC -shared native/tests/plugin_fixture.c -o native/build/test-modules/fixture.so
 python3 native/app-sdk/package-app.py native/build/test-modules/fixture.so linux native/build/test-modules/fixture.dmapp
fi
cc -DDESKTOP_PREVIEW -Dd_m3MaxLinearMemoryPages=1024 -std=gnu11 -Wall -Wextra -w -O2 -Inative/desktop/include -Inative/vendor/wasm3 \
 native/main.c native/filesystem.c native/plugins.c native/system.c native/core_installer.c native/system_properties.c native/control_panel.c native/file_dialog.c native/clock_ui.c native/confirm_ui.c native/file_jobs.c native/app_manager.c native/network.c native/network_settings.c native/rename.c native/media.c native/media_player.c native/file_icons.c native/file_types.c native/device_ui.c native/bluetooth_native.c native/preferences.c native/i18n.c native/app_runtime.c native/screensaver.c native/drop_transfer.c native/rdp_server.c native/remote_panel.c native/input_devices.c native/multi_file.c native/wasm_sandbox.c native/wasm_app_runtime.c native/updater.c native/vendor/browser-engine/duktape/src/duktape.c native/pdf/desktop_pdf.c native/desktop/compat.c \
 native/vendor/wasm3/m3_api_libc.c native/vendor/wasm3/m3_api_tracer.c native/vendor/wasm3/m3_bind.c native/vendor/wasm3/m3_code.c native/vendor/wasm3/m3_compile.c native/vendor/wasm3/m3_core.c native/vendor/wasm3/m3_emit.c native/vendor/wasm3/m3_env.c native/vendor/wasm3/m3_exec.c native/vendor/wasm3/m3_function.c native/vendor/wasm3/m3_info.c native/vendor/wasm3/m3_module.c native/vendor/wasm3/m3_parse.c \
 -Inative/vendor/libsmb2/include native/build/smb2-linux/lib/libsmb2.a $(pkg-config --cflags --libs sdl2 SDL2_mixer sndfile SDL2_image SDL2_ttf libpng libcurl freetype2 libwebp) -ljpeg -lbz2 -ldl -lssl -lcrypto -lzstd -lz -lm -o native/build/desktop-mode-linux
exec native/build/desktop-mode-linux
