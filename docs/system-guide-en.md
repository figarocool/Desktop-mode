# Desktop Mode — system guide

Desktop Mode is a C/VitaSDK desktop environment for PS Vita. The shell provides a desktop with movable icons, a Start menu, context menus, a taskbar, and resizable windows. The pointer can be controlled with the analogue sticks, touch, or supported HID devices. Apps have their own windows and taskbar entries; they can be minimized, restored, maximized, and closed.

## Architecture and apps

The native shell manages input, windows, memory, the filesystem, and system services. `.dmapp` modules can contain a WASM app run by the WebAssembly runtime or a native Vita module. WASM apps use the Desktop SDK host API and run inside the WASM runtime sandbox; native modules share the shell process and require greater trust. Apps are not separate Vita processes.

Bundled apps are Notepad, Browser, Calculator, Paint, Image Viewer, PDF Viewer, Media Player, Console, Task Manager, Network, Solitaire, Minesweeper, and Counter. Third-party apps can register through the SDK and `.dmapp` package format without rebuilding the shell. See the complete references: [Desktop Mode API, English](desktop-api-en.md) and [API Desktop Mode, italiano](desktop-api.md).

## Files and associations

Computer shows detected drives, including `ux0:`. Explorer supports navigation, multi-selection, copy, move, rename, deletion through the Trash, and drag-and-drop. Long operations show progress. File associations open extensions with an installed app; image, text, and PDF apps are bundled.

## Bundled apps

Notepad edits text with selection and clipboard operations. Paint supports drawing and editing a canvas that can extend beyond the visible area using scrollbars. Image Viewer opens raster formats supported by the bundled decoder. PDF Viewer uses the library included in the project; see [PDF support](pdf-viewer.md).

Browser uses the internal engine: HTML5/CSS parsing through Lexbor, JavaScript through Duktape, PNG/JPEG/WebP images, forms, tabs, history, and bookmarks. Its layout and web APIs are partial and do not provide full desktop-browser compatibility. Media Player uses available bundled decoders and platform backends; see [media formats and limitations](media-player.md).

Console provides file and directory commands. Task Manager shows windows and memory tracked by compatible apps. Solitaire and Minesweeper are WASM games. Control Panel manages personalization, language, settings, installed apps, and devices.

## Network, Bluetooth, and remote desktop

Local networking can discover IPv4 hosts and access SMB2/3 shares for browsing and individual-file transfers. The Wi-Fi interface can list access points and submit a connection request with a password. DHCP/static-IP support and some functions depend on firmware APIs and still need console testing. Bluetooth uses Vita userland APIs to scan for and pair supported devices.

The Remote Desktop server shares the shell display with remote mouse and keyboard input. RDP support is partial: NLA/CredSSP, audio, and remote clipboard are unavailable; real Wi-Fi connections still need console testing. See [RDP](remote-desktop.md) and [network and USB status](status-network-usb.md).

## Updates

At Vita startup, the updater checks the latest GitHub release. It downloads changed `.dmapp` packages, verifies their checksums and format, then loads them from `ux0:/data/desktop-mode/apps/` ahead of the copies embedded in the VPK. A new core VPK is downloaded and verified at `ux0:/data/desktop-mode/updates/desktop-mode.vpk`; installing it requires the console's homebrew installer and a restart. See [release updates](aggiornamenti-release.md).

## Build and tests

On Linux, `./scripts/desktop-preview.sh` builds and launches the desktop preview. Vita builds require VitaSDK and CMake; build the `desktop-mode.vpk-vpk` target in `native/build`. The main checks are `bash scripts/test-wasm-app-runtime.sh` and `bash scripts/test-widgets.sh`. The preview cannot emulate every Vita hardware API: Wi-Fi, Bluetooth, input, and VPK installation must be tested on the console.
