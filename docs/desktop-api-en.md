# Desktop Mode API v3 — English reference

This document describes the public C plug-in API and the WebAssembly app ABI. The Italian reference is [desktop-api.md](desktop-api.md). Both interfaces let an app use the desktop services; they do not turn platform-specific native binaries into portable apps.

## Native C plug-ins

Include `desktop_plugin.h`, define a `DmApp` descriptor and export it with `DM_EXPORT_APP`. The host allocates the app state and calls `open`, `draw`, input callbacks, `tick`, and `close`. Each app declares its file extensions in the descriptor. See the [SDK build guide](../native/app-sdk/README-wasm.md) and `native/examples/hello.c`.

### Window and drawing services

| API | Purpose |
| --- | --- |
| `dm_register_app`, `dm_launch`, `dm_focus`, `dm_close`, `dm_maximize` | Register an app and manage its windows. `dm_close` returns whether closure was accepted. |
| `dm_rect`, `dm_line` | Draw filled rectangles and lines. Drawing is clipped to the active app window. |
| `dm_text`, `dm_text_center`, `dm_text_width` | Draw translatable UI labels, centered text, and measure UI text. |
| `dm_text_raw`, `dm_text_width_raw`, `dm_text_raw_scaled`, `dm_text_width_raw_scaled` | Draw or measure user data such as paths and document contents without translating it. |
| `dm_ui_scale`, `dm_ui_transform_rect`, `dm_ui_untransform_window_pointer` | Convert geometry and pointer coordinates using the configured UI scale. |
| `dm_status` | Show a short desktop status message. |

`DmWindow` contains window state, geometry, restore geometry, the app descriptor and app-owned state. `DmApp` supplies the API version, ID, title, state size, callbacks and supported extensions. Input key constants are `DM_KEY_*`; `DM_KEY_SHIFT` is an additional modifier flag.

### Text, dialogs, and clipboard

| API / type | Purpose |
| --- | --- |
| `dm_prompt(title, initial, callback, context)` | Open the system text prompt. The callback receives the accepted UTF-8 text; cancel does not call it. |
| `DmFileDialogOptions`, `dm_file_dialog` | Open or save a file using a title, starting directory, filename and extension filter. Result callbacks receive the selected path and replacement permission. |
| `dm_confirm`, `DmConfirmResult` | Ask for confirmation asynchronously. The callback receives accepted/rejected. |
| `dm_clipboard_text_set`, `dm_clipboard_text_get` | Set or read the shared text clipboard. |
| `dm_localize(it, en, es)`, `dm_ui_translate` | Select a localized app string or translate a UI catalog label. |

### Files and operations

| API / type | Purpose |
| --- | --- |
| `dm_fs_read`, `dm_fs_write` | Read and write files using Vita mount paths or paths supported by the host platform. `exclusive` prevents overwriting an existing file. |
| `dm_fs_list`, `DmDirectoryEntry` | Enumerate a directory in pages. Entries contain a name, byte size and directory flag. |
| `dm_fs_is_directory`, `dm_fs_mkdir`, `dm_fs_join`, `dm_fs_parent`, `dm_fs_writable` | Inspect, create and compose paths. The parent of a path is written back by `dm_fs_parent`. |
| `dm_fs_copy`, `dm_fs_rename` | Copy or rename files and directories synchronously. Rename does not replace an existing destination. |
| `dm_fs_path_update` | Update an app-held path after the desktop renames that file or one of its parent directories. Consume revisions regularly; the rename journal retains the most recent 32 events. |
| `dm_copy_async`, `dm_trash_async`, `DmOperationResult` | Start desktop copy or trash operations with completion result. A zero return means the operation started; callback values are success `1`, cancelled `0`, or error `-1`. |
| `dm_associate_extension`, `dm_open_file` | Register an extension association or ask the desktop to open a path with its registered app. |

Use `dm_file_dialog` and `dm_confirm` for user decisions instead of assuming a path or replacing data without consent. Async callback context must remain valid until its callback runs.

### Images, tasks, and system information

| API / type | Purpose |
| --- | --- |
| `dm_image_load`, `dm_image_create`, `dm_image_update`, `dm_image_read` | Load an image or create/update/read its RGBA pixel buffer. Image handles are opaque and must not be used after `dm_image_free`. |
| `dm_image_size`, `dm_image_draw`, `dm_image_draw_clipped`, `dm_image_free` | Query, draw, clip and release an image handle. |
| `dm_image_save_png` | Encode and save an RGBA buffer as PNG. The buffer contains `width * height` pixels. |
| `dm_tasks`, `DmTaskInfo`, `dm_app_memory` | Inspect Desktop Mode app windows and app memory use. These are not a list of all Vita OS processes. |
| `dm_system_info`, `DmSystemInfo` | Read console model, firmware, battery, clock rates, RAM/storage figures and network address where available. |
| `dm_clock_ms` | Read the monotonic millisecond clock. |
| `dm_tray_set` | Add, update or remove an app's taskbar tray entry. |
| `dm_memory_register`, `DmMemoryUsage` | Register an app-provided memory usage callback. |
| `dm_register_screensaver`, `DmScreenSaver` | Register a native screen saver and its lifecycle callbacks. |

### Host API table

`dm_host_api` is the versioned function table passed to native plug-ins. Its fields mirror the services above; check `api_version` and `struct_size` before using appended fields. API v3 appends raw text/measurement, UI translation and proportional scrollbar drawing while preserving earlier field offsets.

## WebAssembly apps

WASM apps include `wasm_app.h`, import only functions from the `desktop` module and export `dm_app_abi_version`, `dm_app_init`, `dm_app_draw`, `dm_app_click` and `dm_app_close`. Optional exports include text/file results, keyboard input, ticking, menu commands, dirty-document checks and event handling. Coordinates are local to the app's client area; strings are UTF-8 in the module's linear memory. The host bounds-checks memory pointers, and image/file handles are opaque integers.

Bundled WASM apps use the `.dmapp` suffix and their package header identifies the payload type. Third-party native `.dmapp` plug-ins remain supported; `.suprx` identifies a Vita kernel module. Each WASM app instance has its own runtime and memory, while native host services remain shared with the desktop process.

| Import group | Functions in `wasm_app.h` | Purpose |
| --- | --- | --- |
| Drawing and text | `rect`, `scrollbar_draw`, `text`, `text_scaled`, `text_width`, `text_width_scaled` | Draw clipped UI and measure text using the desktop font settings. |
| Pointer and task manager | `pointer_state`, `task_count`, `task_get`, `task_memory`, `task_action` | Read pointer state and inspect/control Desktop Mode windows. |
| Images | `image_load`, `image_size`, `image_read`, `image_draw`, `image_draw_clipped`, `image_free`, `image_create`, `image_update`, `image_save_png` | Load, inspect, edit, display and save image data. Read capacity is in bytes. |
| File system | `fs_list_text`, `fs_is_directory`, `fs_mkdir`, `fs_copy`, `fs_rename`, `fs_path_update`, `fs_read`, `fs_write` | Browse and modify files through bounded desktop host calls. |
| File dialogs and prompts | `file_dialog`, `file_dialog_at`, `text_prompt`, `confirm`, `open_file` | Use shared file selection, text entry, confirmation and file association services. Results arrive through app callbacks. |
| Clipboard and app launch | `clipboard_set`, `clipboard_get`, `launch` | Share text or request another registered app to launch. |
| Async operations | `copy_async`, `trash_async` | Start copy/trash jobs; completion arrives through the app event callback. |
| Network and browser | `network_info`, `network_row`, `network_action`, `network_select`, `http_start`, `http_post`, `http_poll`, `http_error`, `http_url`, `js_eval`, `js_output` | Query/control the network service and use bounded HTTP and embedded JavaScript services. HTTP body maximum is `DM_WASM_HTTP_MAX_BODY`. |
| PDF | `pdf_open`, `pdf_close`, `pdf_render` | Open a PDF and render a page into a caller-provided pixel buffer. |
| Media | `media_open`, `media_action`, `media_status`, `media_time_ms`, `media_duration_ms`, `media_seek_ms`, `media_set_volume`, `media_get_volume`, `media_set_speed`, `media_metadata` | Control the host media service and read playback state and tags. |

Imports are capabilities, not direct system calls. An import can be absent on older hosts; apps should check its availability when it is optional and handle an error result. The host validates pointers and capacities before reading or writing module memory.

## Build and tests

Build the bundled WASM apps with `bash scripts/build-wasm-apps.sh`. Build a separate native app with `scripts/build-app.sh linux|vita source.c app-id`; Vita builds require VitaSDK. Run `bash scripts/test-wasm-app-runtime.sh` for the bundled app/runtime checks and `bash scripts/test-widgets.sh` for the native and WASM widget helpers.

For detailed app installation, associations, controls and feature limits, see the [Italian API and controls guide](desktop-api.md). The public headers are the source of truth: `native/desktop_api.h`, `native/app-sdk/desktop_plugin.h`, `native/app-sdk/wasm_app.h` and `native/app-sdk/dm_widgets.h`.
