#include "app_runtime.h"
#include <stdio.h>
#include <string.h>
DmWindow*dm_current_window;
static struct{char id[64];DmMemoryUsage read;} providers[24];static int provider_count;
static struct{void*image;char id[64];uint64_t bytes;} images[128];
static struct{DmWindow*window;char label[24];} tray[4];
int dm_memory_register(const char*id,DmMemoryUsage callback){if(!id||strlen(id)>=64||!callback)return -1;for(int i=0;i<provider_count;i++)if(!strcmp(providers[i].id,id)){providers[i].read=callback;return 0;}if(provider_count==24)return -1;snprintf(providers[provider_count].id,64,"%s",id);providers[provider_count++].read=callback;return 0;}
uint64_t dm_app_memory(DmWindow*w){if(!w||!w->used)return 0;const char*id=w->app->id;uint64_t bytes=0;DmTaskInfo tasks[DM_MAX_WINDOWS];int n=dm_tasks(tasks,DM_MAX_WINDOWS);for(int i=0;i<n;i++)if(!strcmp(tasks[i].window->app->id,id))bytes+=tasks[i].window->app->state_size;for(int i=0;i<provider_count;i++)if(!strcmp(providers[i].id,id))bytes+=providers[i].read();for(int i=0;i<128;i++)if(images[i].image&&!strcmp(images[i].id,id))bytes+=images[i].bytes;return bytes;}
void dm_memory_image(void*image,uint64_t bytes){if(!image||!dm_current_window||!dm_current_window->used)return;for(int i=0;i<128;i++)if(!images[i].image){images[i].image=image;images[i].bytes=bytes;snprintf(images[i].id,64,"%s",dm_current_window->app->id);return;}}
void dm_memory_image_free(void*image){for(int i=0;i<128;i++)if(images[i].image==image)memset(&images[i],0,sizeof(images[i]));}
int dm_tray_set(DmWindow*w,const char*label,int visible){if(!w||!w->used)return -1;for(int i=0;i<4;i++)if(tray[i].window==w){if(!visible)memset(&tray[i],0,sizeof(tray[i]));else snprintf(tray[i].label,24,"%.23s",label?label:w->app->title);return 0;}if(!visible)return 0;for(int i=0;i<4;i++)if(!tray[i].window){tray[i].window=w;snprintf(tray[i].label,24,"%.23s",label?label:w->app->title);return 0;}return -1;}
void dm_runtime_closed(DmWindow*w){dm_tray_set(w,NULL,0);if(dm_current_window==w)dm_current_window=NULL;}
void dm_runtime_draw_tray(void){for(int i=0;i<4;i++)if(tray[i].window&&tray[i].window->used){dm_rect(550+i*30,507,28,34,DM_COLOR(49,80,122,255));char label[4];snprintf(label,4,"%.3s",tray[i].label);dm_text(553+i*30,530,label,DM_COLOR(255,255,255,255));}}
int dm_runtime_tray_click(int x,int y){if(y<504||x<550||x>=670)return 0;int i=(x-550)/30;if(!tray[i].window)return 0;dm_focus(tray[i].window);return 1;}
