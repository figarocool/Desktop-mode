# Desktop Mode WebAssembly app ABI

For the complete bilingual API catalog, see [`docs/desktop-api.md`](../../docs/desktop-api.md) (Italian) and [`docs/desktop-api-en.md`](../../docs/desktop-api-en.md) (English).

The bundled `wasm3` runtime executes WebAssembly 1.0 modules inside the desktop
process. An app owns its linear memory and state, receives lifecycle calls
through named exports, and can call only explicitly linked `desktop` imports.
Native Vita pointers and kernel symbols are never passed into a module.

All thirteen bundled app front ends are WASM modules: Notepad, counter,
calculator, task manager, image preview, Paint, Console, Browser, PDF, network,
Solitaire, Minesweeper, and Media Player. The host draws a contextual File/Edit/View/Help
menu bar for every WASM app. Apps may export `dm_app_menu(int32_t command)`
to implement their own menu actions; the action IDs are declared as
`DM_WASM_MENU_*` in `wasm_app.h`. Notepad uses classic File, Edit, Format,
View, and Help menus, and exports dirty-document hooks so the host can confirm
before discarding edits on close. On this machine
Clang 18 and `wasm-ld-15` are installed. Invoke the linker directly because
Clang's driver passes an option unsupported by the installed linker:

```sh
clang --target=wasm32 -std=c99 -O2 -nostdlib -fno-builtin \
  -c -o /tmp/counter.o examples/wasm-counter.c
wasm-ld-15 --no-entry --allow-undefined \
  --export=dm_app_abi_version --export=dm_app_init \
  --export=dm_app_draw --export=dm_app_click --export=dm_app_close \
  -o counter.wasm /tmp/counter.o
```

The desktop keeps the `.dmapp` extension. Package headers identify WASM
payloads, and the loader rejects invalid modules before adding them to Start.
Each window gets its own wasm3 runtime, 4 MiB linear-memory limit, 64 KiB
stack, and one-million-instruction budget per callback. Host imports cover
drawing, keyboard/text input, clipboard, bounded file access, file selection,
task management, image editing and PNG output, filesystem operations,
asynchronous copy/trash confirmations, and HTTP access.

Browser networking and PDF page rendering are host services; their user-facing
app logic and window state run in WASM. Those native services remain part of
the trusted desktop process, so this migration does not isolate faults inside
libcurl or the PDF renderer. The current WASM Browser extracts readable text
from HTML; CSS layout, JavaScript, forms, tabs, and favorites are not in this
port yet. Native app plugins are no longer packaged for the nine bundled apps;
third-party native `.dmapp` support remains available.

## Classic controls SDK

Apps can include `dm_widgets.h` for reusable Windows 95-style controls. Native
modules include it after `desktop_plugin.h`; WASM modules define
`DM_WIDGETS_WASM` and include it after `wasm_app.h`. Drawing and click helpers
cover buttons, checkboxes, radio buttons, edit fields, list boxes, combos,
progress bars, sliders, tabs, list-view rows, tree items, status bars, toolbars,
spin controls, tooltips, hotkeys and simple animation frames. The app owns the
values and calls hit-testing/update helpers from its click callback. Run
`sh scripts/test-widgets.sh` to compile-check both targets and exercise basic
interactions.

Il toolkit offre i controlli classici riutilizzabili in `dm_widgets.h`. Per le listbox usare `dmw_listbox_ex`, `dmw_listbox_metrics` e `dmw_listbox_scroll_click` per gestire selezione e scrollbar verticali/orizzontali. Per gli alberi usare `DmWidgetTreeNode` e `dmw_treeview`; il modulo fornisce i nodi visibili e aggiorna la struttura quando l'utente espande o chiude un ramo.

La combo a discesa usa `dmw_combo_ex`/`dmw_combo_ex_click`; per input modificabile affiancare `dmw_edit`. Gli helper condividono layout e hit testing, mentre il modulo conserva stato, eventi e dati.
