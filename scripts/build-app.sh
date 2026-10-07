#!/bin/sh
# Standalone app build. Does not rebuild or modify the desktop executable.
set -eu
if [ "$#" -ne 3 ]; then echo "Uso: $0 linux|vita sorgente.c nome-app" >&2; exit 1; fi
platform=$1
source_path=$(realpath "$2")
app_name=$3
case "$app_name" in *[!a-zA-Z0-9_-]*|'') echo "Nome app non valido" >&2; exit 1;; esac
cd "$(dirname "$0")/.."
case "$platform" in
 linux) cmake -S native/app-sdk -B "native/build/plugins/linux-$app_name" -DDM_APP_LINUX=ON -DDM_APP_SOURCE="$source_path" -DDM_APP_NAME="$app_name" -DDM_APP_NEWLIB="${DM_APP_NEWLIB:-OFF}" -DDM_APP_LIBRARIES="${DM_APP_LIBRARIES:-}" -DDM_APP_INCLUDE_DIRS="${DM_APP_INCLUDE_DIRS:-}"; cmake --build "native/build/plugins/linux-$app_name"; mkdir -p native/desktop/demo/ux0/data/desktop-mode/apps; cp "native/build/plugins/linux-$app_name/$app_name.dmapp" "native/desktop/demo/ux0/data/desktop-mode/apps/$app_name.dmapp";;
 vita) VITASDK=${VITASDK:-/usr/local/vitasdk}; export VITASDK; cmake -S native/app-sdk -B "native/build/plugins/vita-$app_name" -DDM_APP_LINUX=OFF -DDM_APP_SOURCE="$source_path" -DDM_APP_NAME="$app_name" -DDM_APP_NEWLIB="${DM_APP_NEWLIB:-OFF}" -DDM_APP_LIBRARIES="${DM_APP_LIBRARIES:-}" -DDM_APP_INCLUDE_DIRS="${DM_APP_INCLUDE_DIRS:-}"; cmake --build "native/build/plugins/vita-$app_name"; echo "Copia native/build/plugins/vita-$app_name/$app_name.dmapp in ux0:/data/desktop-mode/apps/";;
 *) echo "Piattaforma non valida" >&2; exit 1;;
esac
