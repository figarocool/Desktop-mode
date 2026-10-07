#ifndef DM_WASM_APP_H
#define DM_WASM_APP_H

/*
 * Desktop Mode WebAssembly app ABI, version 1.
 *
 * Modules import only the functions declared below from "desktop" and export
 * dm_app_abi_version, dm_app_init, dm_app_draw, dm_app_click, and dm_app_close.
 * All coordinates are local to the app client area. Text pointers refer to
 * UTF-8, NUL-terminated strings in the module's linear memory.
 *
 * The host validates every pointer/length against the module's linear memory.
 * Handles are opaque integers, never native pointers. No Vita SDK symbols are
 * available to a WebAssembly module.
 */
#include <stdint.h>
#include "network_wire.h"

#define DM_WASM_APP_ABI_VERSION 1
#define DM_WASM_HTTP_MAX_BODY (8u * 1024u * 1024u)

#if defined(__wasm__)
#define DM_WASM_EXPORT(name) __attribute__((export_name(name)))
#define DM_WASM_IMPORT(name) __attribute__((import_module("desktop"), import_name(name)))
#else
#define DM_WASM_EXPORT(name)
#define DM_WASM_IMPORT(name)
#endif

enum {
    DM_WASM_COLOR_TEXT = 0xFF182637u,
    DM_WASM_COLOR_BUTTON = 0xFFECD3B8u,
    DM_WASM_COLOR_HIGHLIGHT = 0xFFF6C397u
};

/* Host drawing imports. The host clips all drawing to the app client area. */
DM_WASM_IMPORT("rect") void dm_host_rect(int32_t x, int32_t y,
                                            int32_t width, int32_t height,
                                            uint32_t rgba);
DM_WASM_IMPORT("scrollbar_draw") void dm_host_scrollbar_draw(int32_t x,int32_t y,int32_t length,int32_t thickness,int32_t vertical,int32_t content,int32_t viewport,int32_t offset);
DM_WASM_IMPORT("text") void dm_host_text(int32_t x, int32_t baseline,
                                            const char *utf8,
                                            uint32_t rgba);
DM_WASM_IMPORT("text_scaled") void dm_host_text_scaled(int32_t x,int32_t baseline,
                                            const char *utf8,uint32_t rgba,
                                            int32_t size_percent);
DM_WASM_IMPORT("text_width") int32_t dm_host_text_width(const char *utf8);
DM_WASM_IMPORT("text_width_scaled") int32_t dm_host_text_width_scaled(const char *utf8,
                                            int32_t size_percent);
DM_WASM_IMPORT("task_count") int32_t dm_host_task_count(void);
DM_WASM_IMPORT("task_get") int32_t dm_host_task_get(int32_t index,
                                                      char *title,
                                                      int32_t capacity);
DM_WASM_IMPORT("task_memory") int32_t dm_host_task_memory(int32_t index);
DM_WASM_IMPORT("task_action") int32_t dm_host_task_action(int32_t index,
                                                           int32_t action);
DM_WASM_IMPORT("image_load") int32_t dm_host_image_load(const char *path);
DM_WASM_IMPORT("image_size") int32_t dm_host_image_size(int32_t handle,
                                                          uint32_t *width,
                                                          uint32_t *height);
DM_WASM_IMPORT("image_read") int32_t dm_host_image_read(int32_t handle,uint32_t *pixels,int32_t capacity);
DM_WASM_IMPORT("image_draw") void dm_host_image_draw(int32_t handle,
                                                       int32_t x,int32_t y,
                                                       int32_t width,int32_t height);
DM_WASM_IMPORT("image_draw_clipped") void dm_host_image_draw_clipped(int32_t handle,int32_t x,int32_t y,int32_t width,int32_t height,int32_t clip_x,int32_t clip_y,int32_t clip_width,int32_t clip_height);
DM_WASM_IMPORT("image_free") void dm_host_image_free(int32_t handle);
DM_WASM_IMPORT("image_create") int32_t dm_host_image_create(int32_t width,int32_t height,const uint32_t *pixels);
DM_WASM_IMPORT("image_update") void dm_host_image_update(int32_t handle,const uint32_t *pixels);
DM_WASM_IMPORT("image_save_png") int32_t dm_host_image_save_png(const char *path,int32_t width,int32_t height,const uint32_t *pixels,int32_t exclusive);
DM_WASM_IMPORT("pointer_state") int32_t dm_host_pointer_state(int32_t *x,int32_t *y,int32_t *held);
DM_WASM_IMPORT("fs_list_text") int32_t dm_host_fs_list_text(const char *path,char *output,int32_t capacity,int32_t offset);
DM_WASM_IMPORT("fs_is_directory") int32_t dm_host_fs_is_directory(const char *path);
DM_WASM_IMPORT("fs_mkdir") int32_t dm_host_fs_mkdir(const char *path);
DM_WASM_IMPORT("fs_copy") int32_t dm_host_fs_copy(const char *source,const char *destination);
DM_WASM_IMPORT("fs_rename") int32_t dm_host_fs_rename(const char *source,const char *destination);
DM_WASM_IMPORT("fs_path_update") void dm_host_fs_path_update(char *path,int32_t capacity,uint64_t *seen_revision);
DM_WASM_IMPORT("open_file") void dm_host_open_file(const char *path);
DM_WASM_IMPORT("launch") int32_t dm_host_launch(const char *id,const char *argument);
DM_WASM_IMPORT("confirm") int32_t dm_host_confirm(const char *title,const char *message);
DM_WASM_IMPORT("copy_async") int32_t dm_host_copy_async(const char *source,const char *destination);
DM_WASM_IMPORT("trash_async") int32_t dm_host_trash_async(const char *path);
DM_WASM_IMPORT("http_start") int32_t dm_host_http_start(const char *url);
DM_WASM_IMPORT("http_post") int32_t dm_host_http_post(const char *url,const char *body,int32_t length);
DM_WASM_IMPORT("http_poll") int32_t dm_host_http_poll(char *data,int32_t capacity);
DM_WASM_IMPORT("http_error") int32_t dm_host_http_error(char *data,int32_t capacity);
DM_WASM_IMPORT("http_url") int32_t dm_host_http_url(char *url,int32_t capacity);
DM_WASM_IMPORT("js_eval") int32_t dm_host_js_eval(const char *source,int32_t length);
DM_WASM_IMPORT("js_output") int32_t dm_host_js_output(char *output,int32_t capacity);
DM_WASM_IMPORT("pdf_open") int32_t dm_host_pdf_open(const char *path,const char *password,char *error,int32_t error_capacity);
DM_WASM_IMPORT("pdf_close") void dm_host_pdf_close(void);
DM_WASM_IMPORT("pdf_render") int32_t dm_host_pdf_render(int32_t page,int32_t zoom,int32_t ox,int32_t oy,int32_t width,int32_t height,uint32_t *pixels,int32_t *max_x,int32_t *max_y,char *error,int32_t error_capacity);
DM_WASM_IMPORT("fs_read") int32_t dm_host_fs_read(const char *path,void *data,int32_t capacity);
DM_WASM_IMPORT("fs_write") int32_t dm_host_fs_write(const char *path,const void *data,int32_t length,int32_t exclusive);
DM_WASM_IMPORT("clipboard_set") void dm_host_clipboard_set(const char *text);
DM_WASM_IMPORT("clipboard_get") int32_t dm_host_clipboard_get(char *text,int32_t capacity);
DM_WASM_IMPORT("file_dialog") int32_t dm_host_file_dialog(const char *title,const char *extensions,int32_t save,char *output,int32_t capacity);
DM_WASM_IMPORT("file_dialog_at") int32_t dm_host_file_dialog_at(const char *title,const char *extensions,int32_t save,const char *initial_directory,char *output,int32_t capacity);
DM_WASM_IMPORT("text_prompt") int32_t dm_host_text_prompt(const char *title,const char *initial,char *output,int32_t capacity);
DM_WASM_IMPORT("network_info") int32_t dm_host_network_info(DmNetworkInfo *info,int32_t capacity);
DM_WASM_IMPORT("network_row") int32_t dm_host_network_row(int32_t index,DmNetworkRow *row,int32_t capacity);
DM_WASM_IMPORT("network_action") int32_t dm_host_network_action(int32_t action);
DM_WASM_IMPORT("network_select") int32_t dm_host_network_select(int32_t index,int32_t activate);
/* Optional host media service: playback is performed outside the WASM module. */
DM_WASM_IMPORT("media_open") int32_t dm_host_media_open(const char *path);
DM_WASM_IMPORT("media_action") int32_t dm_host_media_action(int32_t action);
DM_WASM_IMPORT("media_status") int32_t dm_host_media_status(void);
DM_WASM_IMPORT("media_time_ms") int32_t dm_host_media_time_ms(void);
DM_WASM_IMPORT("media_duration_ms") int32_t dm_host_media_duration_ms(void);
DM_WASM_IMPORT("media_seek_ms") int32_t dm_host_media_seek_ms(int32_t milliseconds);
DM_WASM_IMPORT("media_set_volume") int32_t dm_host_media_set_volume(int32_t percent);
DM_WASM_IMPORT("media_get_volume") int32_t dm_host_media_get_volume(void);
DM_WASM_IMPORT("media_set_speed") int32_t dm_host_media_set_speed(int32_t percent);
DM_WASM_IMPORT("media_metadata") int32_t dm_host_media_metadata(char *artist,int32_t artist_capacity,char *title,int32_t title_capacity);
enum {DM_WASM_KEY_BACKSPACE=1,DM_WASM_KEY_ENTER,DM_WASM_KEY_LEFT,DM_WASM_KEY_RIGHT,DM_WASM_KEY_UP,DM_WASM_KEY_DOWN,DM_WASM_KEY_SAVE,DM_WASM_KEY_COPY,DM_WASM_KEY_PASTE,DM_WASM_KEY_CUT,DM_WASM_KEY_SELECT_ALL,DM_WASM_KEY_DELETE,DM_WASM_KEY_HOME,DM_WASM_KEY_END,DM_WASM_KEY_SCROLL_UP,DM_WASM_KEY_SCROLL_DOWN};
#define DM_WASM_KEY_SHIFT 0x100
enum {DM_WASM_MENU_NEW=1,DM_WASM_MENU_OPEN,DM_WASM_MENU_SAVE,DM_WASM_MENU_SAVE_AS,DM_WASM_MENU_EXIT,DM_WASM_MENU_UNDO,DM_WASM_MENU_CUT,DM_WASM_MENU_COPY,DM_WASM_MENU_PASTE,DM_WASM_MENU_SELECT_ALL,DM_WASM_MENU_ZOOM_IN,DM_WASM_MENU_ZOOM_OUT,DM_WASM_MENU_ABOUT,DM_WASM_MENU_RELOAD,DM_WASM_MENU_PREVIOUS,DM_WASM_MENU_NEXT,DM_WASM_MENU_RESET,DM_WASM_MENU_WRAP,DM_WASM_MENU_STATUS,DM_WASM_MENU_CLEAR,DM_WASM_MENU_INSERT_TEXT,DM_WASM_MENU_MEDIA_TOGGLE_PLAY=21,DM_WASM_MENU_MEDIA_STOP,DM_WASM_MENU_MEDIA_BACK,DM_WASM_MENU_MEDIA_FORWARD,DM_WASM_MENU_MEDIA_PLAYLIST,DM_WASM_MENU_MEDIA_VISUALIZATION,DM_WASM_MENU_IMAGE_SIZE};
enum {DM_WASM_EVENT_CONFIRM=1,DM_WASM_EVENT_OPERATION=2};
enum {DM_WASM_MEDIA_PLAY=1,DM_WASM_MEDIA_PAUSE,DM_WASM_MEDIA_STOP};

/* App lifecycle exports. State belongs to this module instance. */
DM_WASM_EXPORT("dm_app_abi_version") uint32_t dm_app_abi_version(void);
DM_WASM_EXPORT("dm_app_argument_buffer") uint32_t dm_app_argument_buffer(void);
DM_WASM_EXPORT("dm_app_text_buffer") uint32_t dm_app_text_buffer(void);
DM_WASM_EXPORT("dm_app_file_buffer") uint32_t dm_app_file_buffer(void);
DM_WASM_EXPORT("dm_app_file_result") void dm_app_file_result(uint32_t length);
DM_WASM_EXPORT("dm_app_event") void dm_app_event(int32_t type,int32_t result);
DM_WASM_EXPORT("dm_app_init") void dm_app_init(const char *argument);
DM_WASM_EXPORT("dm_app_draw") void dm_app_draw(int32_t width,
                                                int32_t height);
DM_WASM_EXPORT("dm_app_click") void dm_app_click(int32_t x, int32_t y,
                                                  uint32_t buttons);
DM_WASM_EXPORT("dm_app_text") void dm_app_text(uint32_t length);
DM_WASM_EXPORT("dm_app_key") void dm_app_key(int32_t key);
DM_WASM_EXPORT("dm_app_tick") void dm_app_tick(uint32_t elapsed_ms);
/* Optional contextual menu command handler. */
DM_WASM_EXPORT("dm_app_menu") void dm_app_menu(int32_t command);
DM_WASM_EXPORT("dm_app_close") void dm_app_close(void);

#endif
