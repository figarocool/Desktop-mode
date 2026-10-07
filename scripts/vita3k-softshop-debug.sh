#!/usr/bin/env bash
set -uo pipefail

ROOT="$(cd -- "$(dirname -- "$0")/.." && pwd)"
VPK="${1:-$ROOT/native/build/desktop-mode.vpk}"
RUNNER="${VITA3K_RUNNER:-/home/stefano/Scrivania/sorgenti/pac-gal/tools/run_vita3k.sh}"
EMULATOR_LOG="/tmp/pacgal-emutest/cache/Vita3K/vita3k.log"
OUTPUT_LOG="/tmp/desktop-mode-vita3k-debug.log"
LAUNCH_LOG="/tmp/desktop-mode-vita3k-launch.log"

if [[ ! -f "$VPK" ]]; then
    printf 'VPK non trovato: %s\n' "$VPK" >&2
    exit 2
fi
if [[ ! -x "$RUNNER" ]]; then
    printf 'Launcher Vita3K non trovato o non eseguibile: %s\n' "$RUNNER" >&2
    printf 'Imposta VITA3K_RUNNER=/percorso/del/launcher per usare un altro percorso.\n' >&2
    exit 2
fi

printf 'Avvio Desktop Mode in Vita3K: %s\n' "$VPK"
printf 'Chiudi Vita3K dopo la prova; il log verrà salvato in %s\n' "$OUTPUT_LOG"

"$RUNNER" "$VPK" 2>&1 | tee "$LAUNCH_LOG"
runner_status=${PIPESTATUS[0]}

if [[ -f "$EMULATOR_LOG" ]]; then
    cp -- "$EMULATOR_LOG" "$OUTPUT_LOG"
    printf 'Log Vita3K copiato: %s\n' "$OUTPUT_LOG"
else
    cp -- "$LAUNCH_LOG" "$OUTPUT_LOG"
    printf 'Log interno Vita3K non trovato; salvato l’output di avvio: %s\n' "$OUTPUT_LOG"
fi

exit "$runner_status"
