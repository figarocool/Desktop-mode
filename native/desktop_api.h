#ifndef DESKTOP_API_H
#define DESKTOP_API_H
#include <stddef.h>
#include <stdint.h>
/* Avoid Vita ARM-to-Thumb libc veneers for ASCII identifiers and file names. */
int dm_ascii_casecmp(const char*a,const char*b);
#define DM_API_VERSION 3
#define DM_MAX_WINDOWS 8
#define DM_APP_LIMIT 48
#define DM_PATH_MAX 1024
#define DM_TEXT_MAX 32768
#define DM_COLOR(r,g,b,a) ((uint32_t)(r)|((uint32_t)(g)<<8)|((uint32_t)(b)<<16)|((uint32_t)(a)<<24))
typedef struct DmWindow DmWindow;
typedef struct {
 unsigned api_version;
 const char *id,*title;
 size_t state_size;
 void (*open)(DmWindow*,const char*argument);
 void (*draw)(DmWindow*);
 void (*click)(DmWindow*,int x,int y);
 void (*text)(DmWindow*,const char *utf8);
 void (*key)(DmWindow*,int key);
 int (*close)(DmWindow*); /* 0 cancels closure, 1 permits it */
 const char *extensions; /* comma-separated: .txt,.log */
 void (*tick)(DmWindow*,unsigned elapsed_ms);
} DmApp;
struct DmWindow {int used,minimized,maximized,x,y,w,h,restore_x,restore_y,restore_w,restore_h;const DmApp *app;void *state;};
enum {DM_KEY_BACKSPACE=1,DM_KEY_ENTER,DM_KEY_LEFT,DM_KEY_RIGHT,DM_KEY_UP,DM_KEY_DOWN,DM_KEY_SAVE,DM_KEY_COPY,DM_KEY_PASTE,DM_KEY_CUT,DM_KEY_SELECT_ALL,DM_KEY_DELETE,DM_KEY_HOME,DM_KEY_END,DM_KEY_SCROLL_UP,DM_KEY_SCROLL_DOWN,DM_KEY_FILEDROP};
#define DM_KEY_SHIFT 0x100
typedef struct {int x,y,held,focused;} DmPointerState;
void dm_pointer_state(DmWindow*,DmPointerState*);
typedef void (*DmTextResult)(const char*,void*);
int dm_register_app(const DmApp*app);
DmWindow *dm_launch(const char *app_id,const char *argument);
void dm_focus(DmWindow*window);
int dm_close(DmWindow*window);
void dm_maximize(DmWindow*window);
int dm_associate_extension(const char*extension,const char*app_id);
void dm_open_file(const char*path);
int dm_load_plugin(const char*path);
void dm_scan_plugins(void);
int dm_loaded_plugin_count(void);
void dm_rect(int x,int y,int w,int h,uint32_t color);
float dm_ui_scale(void);
void dm_ui_transform_rect(int*x,int*y,int*w,int*h);
void dm_ui_untransform_window_pointer(DmWindow*window,int*x,int*y);
void dm_line(int x1,int y1,int x2,int y2,uint32_t color);
void dm_text(int x,int baseline,const char*,uint32_t color);
/* Draw user-provided text, paths, or document content without UI localization. */
void dm_text_raw(int x,int baseline,const char*,uint32_t color);
int dm_text_width_raw(const char*);
void dm_text_raw_scaled(int x,int baseline,const char*,uint32_t color,int percent);
int dm_text_width_raw_scaled(const char*,int percent);
const char *dm_localize(const char *italian,const char *english,const char *spanish);
const char *dm_ui_translate(const char *italian_ui_string);
void dm_status(const char*message);
void dm_prompt(const char*title,const char*initial,DmTextResult callback,void*context);
enum {DM_FILE_OPEN=0,DM_FILE_SAVE=1};
typedef struct {int mode;const char *title,*initial_directory,*filename,*extensions;} DmFileDialogOptions;
/* Async: result path + explicit permission to replace an existing file. Cancel: no callback. */
typedef void (*DmFileResult)(const char*path,int replace_existing,void*context);
int dm_file_dialog(const DmFileDialogOptions*,DmFileResult,void*context);
typedef void (*DmConfirmResult)(int accepted,void*context);
int dm_confirm(const char*title,const char*message,DmConfirmResult,void*);
int dm_confirm_active(void);
void dm_confirm_draw(void);
void dm_confirm_click(int,int);
void dm_confirm_cancel(void);
int dm_file_dialog_active(void);
void dm_file_dialog_draw(void);
void dm_file_dialog_click(int,int);
void dm_file_dialog_key(int);
void dm_file_dialog_cancel(void);
int dm_text_width(const char*text);
void dm_text_center(int center_x,int baseline,const char*text,uint32_t color);
void dm_clipboard_text_set(const char*text);
const char *dm_clipboard_text_get(void);
/* FS: console mount paths; desktop adapter maps these to demo directories. */
int dm_fs_read(const char*path,char*buffer,size_t capacity);
int dm_fs_write(const char*path,const void*data,size_t length,int exclusive);
int dm_fs_copy(const char*source,const char*destination);
int dm_fs_rename(const char*source,const char*destination);
void dm_fs_path_update(char*path,size_t capacity,uint64_t*seen_revision);
int dm_fs_is_directory(const char*path);
int dm_fs_join(char*out,size_t capacity,const char*directory,const char*name);
int dm_fs_writable(const char*path);
void dm_fs_parent(char*path);
typedef struct {int preview,battery_percent,charging,cpu_mhz,gpu_mhz;uint64_t ram_free,storage_total,storage_free;char model[64],firmware[40],ip[40];} DmSystemInfo;
void dm_system_info(DmSystemInfo*info);
int dm_system_network_init(void);
uint64_t dm_clock_ms(void);
typedef struct { DmWindow *window; char title[96]; int minimized; } DmTaskInfo;
int dm_tasks(DmTaskInfo*,int capacity);
void *dm_image_load(const char*path);
void *dm_image_create(unsigned width,unsigned height,const uint32_t*pixels);
void dm_image_update(void*image,const uint32_t*pixels);
void dm_image_draw(void*image,int x,int y,int width,int height);
void dm_image_draw_clipped(void*image,int x,int y,int width,int height,int clip_x,int clip_y,int clip_width,int clip_height);
void dm_image_size(void*image,unsigned*width,unsigned*height);
int dm_image_read(void*image,uint32_t*pixels,unsigned capacity);
void dm_image_free(void*image);
int dm_image_save_png(const char*path,unsigned width,unsigned height,const uint32_t*pixels,int exclusive);
typedef struct {char name[256];uint64_t size;int directory;} DmDirectoryEntry;
typedef void (*DmOperationResult)(int success,void*context);
int dm_fs_list(const char*directory,DmDirectoryEntry*entries,int capacity,int offset);
int dm_fs_mkdir(const char*path);
int dm_copy_async(const char*source,const char*destination,DmOperationResult,void*);
int dm_trash_async(const char*source,DmOperationResult,void*);
enum {DM_LANG_SYSTEM=-1,DM_LANG_IT=0,DM_LANG_EN=1,DM_LANG_ES=2};
int dm_language(void);
const char*dm_localize(const char*it,const char*en,const char*es);
int dm_tray_set(DmWindow*,const char*label,int visible);
typedef uint64_t (*DmMemoryUsage)(void);
int dm_memory_register(const char*app_id,DmMemoryUsage);
uint64_t dm_app_memory(DmWindow*);
typedef struct {unsigned api_version;const char*id,*title;size_t state_size;void(*open)(void*);void(*draw)(void*,int,int);void(*tick)(void*,unsigned);void(*close)(void*);} DmScreenSaver;
int dm_register_screensaver(const DmScreenSaver*);
typedef struct {
 uint32_t version,struct_size;
 int (*register_app)(const DmApp*);
 DmWindow *(*launch)(const char*,const char*);
 void (*focus)(DmWindow*);
 int (*close)(DmWindow*);
 void (*maximize)(DmWindow*);
 int (*associate_extension)(const char*,const char*);
 void (*open_file)(const char*);
 void (*rect)(int,int,int,int,uint32_t);
 void (*text)(int,int,const char*,uint32_t);
 void (*status)(const char*);
 void (*prompt)(const char*,const char*,DmTextResult,void*);
 void (*clipboard_text_set)(const char*);
 const char *(*clipboard_text_get)(void);
 int (*fs_read)(const char*,char*,size_t);
 int (*fs_write)(const char*,const void*,size_t,int);
 int (*fs_copy)(const char*,const char*);
 int (*fs_is_directory)(const char*);
 int (*fs_join)(char*,size_t,const char*,const char*);
 int (*fs_writable)(const char*);
 void (*fs_parent)(char*);
 void (*system_info)(DmSystemInfo*);
 uint64_t (*clock_ms)(void);
 int (*file_dialog)(const DmFileDialogOptions*,DmFileResult,void*);
 int (*confirm)(const char*,const char*,DmConfirmResult,void*);
 int (*text_width)(const char*);
 void (*text_center)(int,int,const char*,uint32_t);
 void (*pointer_state)(DmWindow*,DmPointerState*);
 int (*fs_rename)(const char*,const char*);
 void (*fs_path_update)(char*,size_t,uint64_t*);
 int (*tasks)(DmTaskInfo*,int);
 void *(*image_load)(const char*);
 void *(*image_create)(unsigned,unsigned,const uint32_t*);
 void (*image_update)(void*,const uint32_t*);
 void (*image_draw)(void*,int,int,int,int);
 void (*image_size)(void*,unsigned*,unsigned*);
 void (*image_free)(void*);
 int (*image_save_png)(const char*,unsigned,unsigned,const uint32_t*,int);
 int (*fs_list)(const char*,DmDirectoryEntry*,int,int);
 int (*fs_mkdir)(const char*);
 int (*copy_async)(const char*,const char*,DmOperationResult,void*);
 int (*trash_async)(const char*,DmOperationResult,void*);
 int (*language)(void);
 const char*(*localize)(const char*,const char*,const char*);
 int (*tray_set)(DmWindow*,const char*,int);
 int (*memory_register)(const char*,DmMemoryUsage);
 uint64_t (*app_memory)(DmWindow*);
 int (*register_screensaver)(const DmScreenSaver*);
 /* Appended in API v3: additive helpers; prior field offsets remain stable. */
 void (*text_raw)(int,int,const char*,uint32_t);
 int (*text_width_raw)(const char*);
 const char *(*ui_translate)(const char*);
 /* Draw a proportional horizontal or vertical scrollbar; offset is in content pixels. */
 /* Appended in API v3: proportional scrollbar drawing for native apps. */
 void (*scrollbar_draw)(int,int,int,int,int,int,int,int);
} DmHostAPI;
void dm_scrollbar_draw(int x,int y,int length,int thickness,int vertical,int content,int viewport,int offset);
extern const DmHostAPI dm_host_api;
#endif
