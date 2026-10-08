cmake_minimum_required(VERSION 3.20)
set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain")
project(desktop_mode C ASM)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
set(DM_PDF_MUPDF_ROOT "" CACHE PATH "MuPDF built for Vita: include/mupdf and lib/libmupdf*.a")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ENABLE_LIBKRB5 OFF CACHE BOOL "" FORCE)
set(ENABLE_GSSAPI OFF CACHE BOOL "" FORCE)
set(ENABLE_LIBDCERPC OFF CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
add_subdirectory(vendor/libsmb2)
add_subdirectory(vendor/wasm3)
add_executable(desktop_mode main.c filesystem.c plugins.c system.c system_properties.c control_panel.c file_dialog.c clock_ui.c confirm_ui.c file_jobs.c app_manager.c network.c network_settings.c netctl_private.S rename.c media.c media_player.c file_icons.c file_types.c device_ui.c bluetooth_native.c preferences.c i18n.c app_runtime.c screensaver.c drop_transfer.c rdp_server.c remote_panel.c input_devices.c multi_file.c wasm_sandbox.c wasm_app_runtime.c openssl_compat.c vendor/browser-engine/duktape/src/duktape.c)
target_sources(desktop_mode PRIVATE updater.c core_installer.c)
target_include_directories(desktop_mode PRIVATE vendor/libsmb2/include)
target_compile_options(desktop_mode PRIVATE -Wall -Wextra -O2 -marm)
target_link_libraries(desktop_mode smb2 m3 -Wl,--start-group curl ssl crypto lzma ppmd z -Wl,--end-group zstd vita2d png jpeg webp sharpyuv pthread z m SceDisplay_stub SceGxm_stub SceSysmodule_stub SceHttp_stub SceSsl_stub ScePromoterUtil_stub ScePaf_stub SceAvPlayer_stub SceAudio_stub SceCtrl_stub SceTouch_stub ScePgf_stub ScePvf_stub SceCommonDialog_stub SceAppMgr_stub SceLibKernel_stub SceIofilemgr_stub SceSysmem_stub SceProcessmgr_stub SceKernelModulemgr_stub ScePower_stub SceNetCtl_stub SceNet_stub SceRtc_stub SceAVConfig_stub SceHid_stub SceBt_stub SceWlanBt_stub SceKernelThreadMgr_stub SceAppUtil_stub)

# Optional VitaSDK codecs are linked statically into eboot.bin and therefore
# travel inside the VPK. They are optional so older VitaSDK installations can
# still build the player with the firmware AVPlayer backend.
set(DM_VITA_CODEC_ROOT "$ENV{VITASDK}/arm-vita-eabi" CACHE PATH "VitaSDK prefix containing optional audio codecs")
find_path(DM_SNDFILE_INCLUDE_DIR sndfile.h PATHS "${DM_VITA_CODEC_ROOT}/include" NO_DEFAULT_PATH)
find_library(DM_SNDFILE_LIBRARY sndfile PATHS "${DM_VITA_CODEC_ROOT}/lib" NO_DEFAULT_PATH)
if(DM_SNDFILE_INCLUDE_DIR AND DM_SNDFILE_LIBRARY)
 target_compile_definitions(desktop_mode PRIVATE DM_HAVE_VITA_SNDFILE=1)
 target_include_directories(desktop_mode PRIVATE "${DM_SNDFILE_INCLUDE_DIR}")
 target_link_libraries(desktop_mode -Wl,--start-group "${DM_SNDFILE_LIBRARY}" FLAC vorbisfile vorbisenc vorbis ogg mpg123 mp3lame opus -Wl,--end-group)
 message(STATUS "Vita audio: libsndfile enabled (AIFF/AU/VOC PCM formats)")
else()
 message(STATUS "Vita audio: libsndfile not found; AIFF/AU/VOC software decoding disabled")
endif()
target_compile_definitions(m3 PRIVATE d_m3MaxLinearMemoryPages=1024)
vita_create_self(eboot.bin desktop_mode UNSAFE)
add_executable(dm_core_updater core_updater.c)
target_compile_options(dm_core_updater PRIVATE -Wall -Wextra -O2 -marm)
target_link_libraries(dm_core_updater crypto SceAppMgr_stub ScePromoterUtil_stub SceSysmodule_stub SceIofilemgr_stub SceLibKernel_stub SceKernelModulemgr_stub)
vita_create_self(core-updater-eboot.bin dm_core_updater UNSAFE)
vita_create_vpk(dm-updater.vpk DMUPD0001 core-updater-eboot.bin VERSION "01.00" NAME "Desktop Mode Updater" FILE assets/vitashell-head.bin assets/vitashell-head.bin)
# vita_create_vpk accumulates -a flags in this variable across invocations.
# Keep the helper's private files out of the main VPK's asset list.
set(VITA_PACK_VPK_FLAGS "")
file(GLOB ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.png")
set(PACK_FILES FILE ${CMAKE_CURRENT_SOURCE_DIR}/../docs/desktop-mode.rdp examples/desktop-mode.rdp)
list(APPEND PACK_FILES FILE ${CMAKE_CURRENT_SOURCE_DIR}/assets/vitashell-head.bin assets/vitashell-head.bin)
set(DM_RELEASE_VERSION "dev" CACHE STRING "Version embedded in the VPK")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/release-version.txt" "${DM_RELEASE_VERSION}\n")
list(APPEND PACK_FILES FILE ${CMAKE_CURRENT_BINARY_DIR}/release-version.txt assets/version.txt)
file(GLOB LICENSES "${CMAKE_CURRENT_SOURCE_DIR}/licenses/*.txt")
foreach(LICENSE ${LICENSES})
 get_filename_component(NAME ${LICENSE} NAME)
 list(APPEND PACK_FILES FILE ${LICENSE} licenses/${NAME})
endforeach()
foreach(ASSET ${ASSETS})
 get_filename_component(NAME ${ASSET} NAME)
 list(APPEND PACK_FILES FILE ${ASSET} assets/${NAME})
endforeach()
set(DM_BUNDLED_APPS)
list(APPEND PACK_FILES FILE ${CMAKE_CURRENT_SOURCE_DIR}/assets/pdf-font.ttf assets/pdf-font.ttf)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/browser.dmapp
 COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-browser.wasm wasm ${CMAKE_CURRENT_BINARY_DIR}/browser.dmapp
 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-browser.wasm ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py)
add_custom_target(browser-package ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/browser.dmapp)
list(APPEND PACK_FILES FILE ${CMAKE_CURRENT_BINARY_DIR}/browser.dmapp apps/browser.dmapp)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pdf.dmapp
 COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-pdf.wasm wasm ${CMAKE_CURRENT_BINARY_DIR}/pdf.dmapp
 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-pdf.wasm ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py)
add_custom_target(pdf-package ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/pdf.dmapp)
list(APPEND PACK_FILES FILE ${CMAKE_CURRENT_BINARY_DIR}/pdf.dmapp apps/pdf.dmapp)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/network.dmapp
 COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-network.wasm wasm ${CMAKE_CURRENT_BINARY_DIR}/network.dmapp
 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-network.wasm ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py)
add_custom_target(network-package ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/network.dmapp)
list(APPEND PACK_FILES FILE ${CMAKE_CURRENT_BINARY_DIR}/network.dmapp apps/network.dmapp)
foreach(APP solitaire minesweeper)
 add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${APP}.dmapp
  COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-${APP}.wasm wasm ${CMAKE_CURRENT_BINARY_DIR}/${APP}.dmapp
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-${APP}.wasm ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py)
 add_custom_target(${APP}-package ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${APP}.dmapp)
 list(APPEND PACK_FILES FILE ${CMAKE_CURRENT_BINARY_DIR}/${APP}.dmapp apps/${APP}.dmapp)
endforeach()
if(NOT DM_PDF_MUPDF_ROOT)
 add_library(desktop_pdf STATIC pdf/desktop_pdf.c)
 target_include_directories(desktop_pdf PUBLIC apps PRIVATE "$ENV{VITASDK}/arm-vita-eabi/include/freetype2")
 target_compile_options(desktop_pdf PRIVATE -Wall -Wextra -O2)
endif()
if(DM_PDF_MUPDF_ROOT)
 target_sources(desktop_mode PRIVATE apps/pdf_engine.c)
 target_include_directories(desktop_mode PRIVATE "${DM_PDF_MUPDF_ROOT}/include")
 target_link_libraries(desktop_mode "${DM_PDF_MUPDF_ROOT}/lib/libmupdf.a" "${DM_PDF_MUPDF_ROOT}/lib/libmupdf-third.a")
else()
 target_link_libraries(desktop_mode desktop_pdf freetype jpeg png bz2)
endif()
foreach(APP ${DM_BUNDLED_APPS})
 add_executable(dm_${APP}_module apps/${APP}.c)
 target_link_options(dm_${APP}_module PRIVATE -nostdlib -Wl,-q -Wl,-e,module_start)
 if(APP STREQUAL "pdf")
  if(DM_PDF_MUPDF_ROOT)
   target_sources(dm_pdf_module PRIVATE apps/pdf_engine.c)
   target_include_directories(dm_pdf_module PRIVATE "${DM_PDF_MUPDF_ROOT}/include")
   set(DM_PDF_LIBS "${DM_PDF_MUPDF_ROOT}/lib/libmupdf.a" "${DM_PDF_MUPDF_ROOT}/lib/libmupdf-third.a")
  else()

   target_include_directories(dm_pdf_module PRIVATE "$ENV{VITASDK}/arm-vita-eabi/include/freetype2")
   set(DM_PDF_LIBS desktop_pdf freetype jpeg png bz2)
  endif()
  target_compile_definitions(dm_pdf_module PRIVATE DM_PLUGIN_NEWLIB)
  target_link_options(dm_pdf_module PRIVATE -Wl,--wrap=__getreent)
  target_link_libraries(dm_pdf_module -Wl,--start-group ${DM_PDF_LIBS} z m c gcc SceNet_stub SceNetCtl_stub SceSysmodule_stub SceRtc_stub SceKernelThreadMgr_stub SceSysmem_stub SceLibKernel_stub SceIofilemgr_stub SceProcessmgr_stub SceKernelModulemgr_stub -Wl,--end-group)
 endif()
 if(APP STREQUAL "browser")
  target_include_directories(dm_${APP}_module PRIVATE "$ENV{VITASDK}/arm-vita-eabi/include/libxml2")
  target_compile_definitions(dm_${APP}_module PRIVATE DM_PLUGIN_NEWLIB)
  target_link_options(dm_${APP}_module PRIVATE -Wl,--wrap=__getreent)
  target_link_libraries(dm_${APP}_module -Wl,--start-group curl xml2 ssl crypto z zstd m c gcc SceNet_stub SceNetCtl_stub SceSysmodule_stub SceRtc_stub SceKernelThreadMgr_stub SceSysmem_stub SceLibKernel_stub SceIofilemgr_stub SceProcessmgr_stub SceKernelModulemgr_stub -Wl,--end-group)
 endif()
 if(NOT APP STREQUAL "browser" AND NOT APP STREQUAL "pdf")
  target_compile_definitions(dm_${APP}_module PRIVATE DM_PLUGIN_NEWLIB)
  target_link_options(dm_${APP}_module PRIVATE -Wl,--wrap=__getreent)
  target_link_libraries(dm_${APP}_module -Wl,--start-group c m gcc SceSysmem_stub SceIofilemgr_stub SceProcessmgr_stub SceKernelModulemgr_stub SceKernelThreadMgr_stub SceNet_stub SceRtc_stub SceLibKernel_stub -Wl,--end-group)
 endif()
 target_compile_definitions(dm_${APP}_module PRIVATE DM_TRACK_MEMORY)
 target_link_options(dm_${APP}_module PRIVATE -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=free)
 target_compile_options(dm_${APP}_module PRIVATE -Wall -Wextra -O2)
 vita_create_self(${APP}.suprx dm_${APP}_module UNSAFE MODULE_ENTRY "module_start,module_stop,module_exit")
 add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${APP}.dmapp
 COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py ${CMAKE_CURRENT_BINARY_DIR}/${APP}.suprx vita ${CMAKE_CURRENT_BINARY_DIR}/${APP}.dmapp
 DEPENDS ${APP}.suprx-self ${CMAKE_CURRENT_BINARY_DIR}/${APP}.suprx ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py)
 add_custom_target(${APP}-package ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${APP}.dmapp)
 list(APPEND PACK_FILES FILE ${CMAKE_CURRENT_BINARY_DIR}/${APP}.dmapp apps/${APP}.dmapp)
endforeach()
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/notepad.dmapp
 COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-notepad.wasm wasm ${CMAKE_CURRENT_BINARY_DIR}/notepad.dmapp
 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-notepad.wasm ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py)
add_custom_target(notepad-package ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/notepad.dmapp)
list(APPEND PACK_FILES FILE ${CMAKE_CURRENT_BINARY_DIR}/notepad.dmapp apps/notepad.dmapp)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/paint.dmapp
 COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-paint.wasm wasm ${CMAKE_CURRENT_BINARY_DIR}/paint.dmapp
 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-paint.wasm ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py)
add_custom_target(paint-package ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/paint.dmapp)
list(APPEND PACK_FILES FILE ${CMAKE_CURRENT_BINARY_DIR}/paint.dmapp apps/paint.dmapp)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/console.dmapp
 COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-console.wasm wasm ${CMAKE_CURRENT_BINARY_DIR}/console.dmapp
 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-console.wasm ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py)
add_custom_target(console-package ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/console.dmapp)
list(APPEND PACK_FILES FILE ${CMAKE_CURRENT_BINARY_DIR}/console.dmapp apps/console.dmapp)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/counter.dmapp
 COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-counter.wasm wasm ${CMAKE_CURRENT_BINARY_DIR}/counter.dmapp
 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-counter.wasm ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py)
add_custom_target(counter-package ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/counter.dmapp)
list(APPEND PACK_FILES FILE ${CMAKE_CURRENT_BINARY_DIR}/counter.dmapp apps/counter.dmapp)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/calculator.dmapp
 COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-calculator.wasm wasm ${CMAKE_CURRENT_BINARY_DIR}/calculator.dmapp
 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-calculator.wasm ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py)
add_custom_target(calculator-package ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/calculator.dmapp)
list(APPEND PACK_FILES FILE ${CMAKE_CURRENT_BINARY_DIR}/calculator.dmapp apps/calculator.dmapp)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/taskmanager.dmapp
 COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-taskmanager.wasm wasm ${CMAKE_CURRENT_BINARY_DIR}/taskmanager.dmapp
 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-taskmanager.wasm ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py)
add_custom_target(taskmanager-package ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/taskmanager.dmapp)
list(APPEND PACK_FILES FILE ${CMAKE_CURRENT_BINARY_DIR}/taskmanager.dmapp apps/taskmanager.dmapp)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/images.dmapp
 COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-images.wasm wasm ${CMAKE_CURRENT_BINARY_DIR}/images.dmapp
 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-images.wasm ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py)
add_custom_target(images-package ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/images.dmapp)
list(APPEND PACK_FILES FILE ${CMAKE_CURRENT_BINARY_DIR}/images.dmapp apps/images.dmapp)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/media.dmapp
 COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-media.wasm wasm ${CMAKE_CURRENT_BINARY_DIR}/media.dmapp
 DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/examples/wasm-media.wasm ${CMAKE_CURRENT_SOURCE_DIR}/app-sdk/package-app.py)
add_custom_target(media-package ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/media.dmapp)
list(APPEND PACK_FILES FILE ${CMAKE_CURRENT_BINARY_DIR}/media.dmapp apps/media.dmapp)
list(APPEND PACK_FILES FILE ${CMAKE_CURRENT_BINARY_DIR}/dm-updater.vpk assets/dm-updater.vpk)
vita_create_vpk(desktop-mode.vpk DSKMODE01 eboot.bin VERSION "01.00" NAME "Desktop Mode App" FILE assets/icon0.png sce_sys/icon0.png FILE assets/cacert.pem assets/cacert.pem ${PACK_FILES})

add_dependencies(desktop-mode.vpk-vpk notepad-package counter-package browser-package calculator-package taskmanager-package paint-package images-package media-package console-package pdf-package network-package solitaire-package minesweeper-package)
add_dependencies(desktop-mode.vpk-vpk dm-updater.vpk-vpk)

add_dependencies(desktop-mode.vpk-vpk pdf-package)
