#include "preferences.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const DmScreenSaver*savers[12];static int count,active,preview_release;static void*state;static const DmScreenSaver*current;static uint64_t idle;
typedef struct{int x,y,dx,dy;unsigned elapsed;} NativeSaver;
static void native_open(void*data){NativeSaver*s=data;s->x=160;s->y=170;s->dx=2;s->dy=1;}
static void native_tick(void*data,unsigned ms){NativeSaver*s=data;s->elapsed+=ms;while(s->elapsed>=16){s->elapsed-=16;s->x+=s->dx;s->y+=s->dy;if(s->x<10||s->x>700)s->dx=-s->dx;if(s->y<30||s->y>490)s->dy=-s->dy;}}
static void native_draw(void*data,int w,int h){NativeSaver*s=data;dm_rect(0,0,w,h,DM_COLOR(0,0,0,255));dm_text(s->x,s->y,"DESKTOP MODE",DM_COLOR(100,180,250,255));}
static const DmScreenSaver native={DM_API_VERSION,"native","Desktop Mode",sizeof(NativeSaver),native_open,native_draw,native_tick,NULL};
int dm_register_screensaver(const DmScreenSaver*s){if(!s||s->api_version!=DM_API_VERSION||!s->id||!s->title||!s->draw||s->state_size>1024*1024||strlen(s->id)>=64||count==12)return -1;for(int i=0;i<count;i++)if(!strcmp(s->id,savers[i]->id))return -1;savers[count++]=s;return 0;}
int dm_saver_count(void){if(!count)dm_register_screensaver(&native);return count;}
const DmScreenSaver*dm_saver_at(int index){dm_saver_count();return index>=0&&index<count?savers[index]:NULL;}
int dm_saver_active(void){return active;}
int dm_saver_wake(void){if(active&&preview_release)return 1;int was=active;if(current&&state&&current->close)current->close(state);free(state);state=NULL;current=NULL;active=0;idle=0;return was;}
void dm_saver_preview(void){preview_release=0;dm_saver_wake();dm_saver_count();current=&native;for(int i=0;i<count;i++)if(!strcmp(dm_preferences.saver,savers[i]->id))current=savers[i];state=calloc(1,current->state_size?current->state_size:1);if(!state){current=NULL;dm_status("Memoria insufficiente");return;}active=1;preview_release=1;if(current->open)current->open(state);}
void dm_saver_tick(unsigned elapsed,int activity,int blocked){if(active&&preview_release){if(!activity)preview_release=0;if(current->tick)current->tick(state,elapsed);return;}if(activity){dm_saver_wake();return;}if(active){if(current->tick)current->tick(state,elapsed);return;}if(blocked||!dm_preferences.saver_enabled){idle=0;return;}idle+=elapsed;if(idle>=(uint64_t)dm_preferences.saver_seconds*1000){dm_saver_preview();preview_release=0;}}
void dm_saver_draw(void){if(active&&current&&current->draw)current->draw(state,960,544);}
void dm_scan_savers(void){dm_saver_count();const char*directory="ux0:/data/desktop-mode/screensavers/";DmDirectoryEntry entries[32];int n=dm_fs_list(directory,entries,32,0);extern int dm_load_saver(const char*);for(int i=0;i<n;i++){const char*extension=strrchr(entries[i].name,'.');if(!entries[i].directory&&extension&&!strcmp(extension,".dmsaver")){char path[DM_PATH_MAX];if(!dm_fs_join(path,sizeof(path),directory,entries[i].name))dm_load_saver(path);}}}
