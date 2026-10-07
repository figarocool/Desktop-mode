#ifndef DESKTOP_PLUGIN_H
#define DESKTOP_PLUGIN_H
#include "../desktop_api.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
static const DmHostAPI *dm_api;
#define dm_register_app dm_api->register_app
#define dm_launch dm_api->launch
#define dm_focus dm_api->focus
#define dm_close dm_api->close
#define dm_maximize dm_api->maximize
#define dm_associate_extension dm_api->associate_extension
#define dm_open_file dm_api->open_file
#define dm_rect dm_api->rect
#define dm_text dm_api->text
#define dm_text_raw dm_api->text_raw
#define dm_text_width_raw dm_api->text_width_raw
#define dm_ui_translate dm_api->ui_translate
#define dm_scrollbar_draw dm_api->scrollbar_draw
#define dm_text_width dm_api->text_width
#define dm_text_center dm_api->text_center
#define dm_status dm_api->status
#define dm_prompt dm_api->prompt
#define dm_file_dialog dm_api->file_dialog
#define dm_confirm dm_api->confirm
#define dm_clipboard_text_set dm_api->clipboard_text_set
#define dm_clipboard_text_get dm_api->clipboard_text_get
#define dm_fs_read dm_api->fs_read
#define dm_fs_write dm_api->fs_write
#define dm_fs_rename dm_api->fs_rename
#define dm_fs_path_update dm_api->fs_path_update
#define dm_fs_copy dm_api->fs_copy
#define dm_fs_is_directory dm_api->fs_is_directory
#define dm_fs_join dm_api->fs_join
#define dm_fs_writable dm_api->fs_writable
#define dm_fs_parent dm_api->fs_parent
#define dm_system_info dm_api->system_info
#define dm_pointer_state dm_api->pointer_state
#define dm_clock_ms dm_api->clock_ms
#define dm_tasks dm_api->tasks
#define dm_image_load dm_api->image_load
#define dm_image_create dm_api->image_create
#define dm_image_update dm_api->image_update
#define dm_image_draw dm_api->image_draw
#define dm_image_size dm_api->image_size
#define dm_image_free dm_api->image_free
#define dm_image_save_png dm_api->image_save_png
#define dm_fs_list dm_api->fs_list
#define dm_fs_mkdir dm_api->fs_mkdir
#define dm_copy_async dm_api->copy_async
#define dm_trash_async dm_api->trash_async
#define dm_language dm_api->language
#define dm_localize dm_api->localize
#define dm_tray_set dm_api->tray_set
#define dm_app_memory dm_api->app_memory
#define dm_register_screensaver dm_api->register_screensaver
#ifdef DM_TRACK_MEMORY
/* Link-time wrappers cover module allocations, including static dependencies. */
typedef struct{void*pointer;size_t size;}DmAllocation;
static DmAllocation dm_allocations[16384];
static uint64_t dm_allocated_bytes;
void*__real_malloc(size_t);void*__real_calloc(size_t,size_t);void*__real_realloc(void*,size_t);void __real_free(void*);
static void dm_allocation_add(void*p,size_t size){if(!p)return;unsigned first=((uintptr_t)p>>4)&16383;int tomb=-1;for(unsigned i=0;i<16384;i++){unsigned n=(first+i)&16383;if(dm_allocations[n].pointer==(void*)1&&tomb<0)tomb=n;if(!dm_allocations[n].pointer){if(tomb>=0)n=tomb;dm_allocations[n]=(DmAllocation){p,size};dm_allocated_bytes+=size;return;}}if(tomb>=0){dm_allocations[tomb]=(DmAllocation){p,size};dm_allocated_bytes+=size;}}
static size_t dm_allocation_remove(void*p){unsigned first=((uintptr_t)p>>4)&16383;for(unsigned i=0;i<16384;i++){unsigned n=(first+i)&16383;if(!dm_allocations[n].pointer)return 0;if(dm_allocations[n].pointer==p){size_t size=dm_allocations[n].size;dm_allocations[n].pointer=(void*)1;dm_allocated_bytes-=size;return size;}}return 0;}
void*__wrap_malloc(size_t size){void*p=__real_malloc(size);dm_allocation_add(p,size);return p;}
void __wrap_free(void*p){if(p)dm_allocation_remove(p);__real_free(p);}
void*__wrap_calloc(size_t n,size_t size){if(size&&n>SIZE_MAX/size)return NULL;void*p=__real_calloc(n,size);dm_allocation_add(p,n*size);return p;}
void*__wrap_realloc(void*p,size_t size){void*next=__real_realloc(p,size);if(next||!size){if(p)dm_allocation_remove(p);dm_allocation_add(next,size);}return next;}
static uint64_t dm_module_memory(void){return dm_allocated_bytes;}
#endif
static int dm_plugin_attach(const DmHostAPI*api,const DmApp*app){
 if(!api||api->version!=DM_API_VERSION||api->struct_size<sizeof(DmHostAPI))return -1;
 dm_api=api;
 int result=dm_register_app(app);
#ifdef DM_TRACK_MEMORY
 if(result>=0)dm_api->memory_register(app->id,dm_module_memory);
#endif
 return result;
}
static __attribute__((unused)) int dm_saver_attach(const DmHostAPI*api,const DmScreenSaver*saver){if(!api||api->version!=DM_API_VERSION||api->struct_size<sizeof(DmHostAPI))return -1;dm_api=api;return dm_register_screensaver(saver);}
#ifdef DM_PLUGIN_LINUX
#define DM_EXPORT_SCREENSAVER(descriptor) __attribute__((visibility("default"))) int dm_plugin_entry(const DmHostAPI*api){return dm_saver_attach(api,&descriptor);}
#define DM_EXPORT_APP(descriptor) __attribute__((visibility("default"))) int dm_plugin_entry(const DmHostAPI*api){return dm_plugin_attach(api,&descriptor);}
#else
#include <psp2/types.h>
#ifdef DM_PLUGIN_NEWLIB
/* A module linking newlib needs its own initialized heap/runtime. */
#ifndef DM_PLUGIN_HEAP_SIZE
#define DM_PLUGIN_HEAP_SIZE (16*1024*1024)
#endif
unsigned int _newlib_heap_size_user = DM_PLUGIN_HEAP_SIZE;
void _init_vita_heap(void);
#include <reent.h>
static struct _reent dm_module_reent;
struct _reent *__wrap___getreent(void){return &dm_module_reent;}
void _free_vita_io(void);void _free_vita_malloc(void);void _free_vita_heap(void);
void _free_vita_newlib(void){_free_vita_io();_free_vita_malloc();_free_vita_heap();}
void _fini(void){}
void _init_vita_malloc(void);void _init_vita_io(void);
static void dm_module_runtime(void){static int initialized;if(initialized)return;initialized=1;_REENT_INIT_PTR(&dm_module_reent);_init_vita_heap();_init_vita_malloc();_init_vita_io();}
#else
static void dm_module_runtime(void){}
#endif
#define DM_EXPORT_SCREENSAVER(descriptor) int module_start(SceSize args,void*argp){if(args!=sizeof(const DmHostAPI*)||!argp)return -1;const DmHostAPI*api;memcpy(&api,argp,sizeof(api));dm_module_runtime();return dm_saver_attach(api,&descriptor);} int module_stop(SceSize a,void*p){(void)a;(void)p;return -1;} int module_exit(SceSize a,void*p){(void)a;(void)p;return 0;}
#define DM_EXPORT_APP(descriptor) int module_start(SceSize args,void*argp){if(args!=sizeof(const DmHostAPI*)||!argp)return -1;const DmHostAPI*api;memcpy(&api,argp,sizeof(api));if(!api||api->version!=DM_API_VERSION||api->struct_size<sizeof(DmHostAPI))return -1;dm_module_runtime();return dm_plugin_attach(api,&descriptor);} int module_stop(SceSize args,void*argp){(void)args;(void)argp;return -1;} int module_exit(SceSize args,void*argp){(void)args;(void)argp;return 0;}
#endif
#endif
