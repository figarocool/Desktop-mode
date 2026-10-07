#include "../app-sdk/desktop_plugin.h"
#include <stdio.h>
#include <string.h>
typedef struct{void*image;unsigned width,height;char path[DM_PATH_MAX];uint64_t revision;} Viewer;
static void load(DmWindow*w,const char*path){Viewer*v=w->state;void*t=dm_image_load(path);if(!t){dm_status("Immagine non valida o memoria insufficiente (PNG/JPEG)");return;}dm_image_free(v->image);v->image=t;dm_image_size(t,&v->width,&v->height);dm_fs_path_update(v->path,sizeof(v->path),&v->revision);snprintf(v->path,sizeof(v->path),"%s",path);}
static void chosen(const char*p,int replace,void*ctx){(void)replace;DmWindow*w=ctx;if(w->used)load(w,p);}
static void open_file(DmWindow*w){DmFileDialogOptions o={DM_FILE_OPEN,"Apri immagine","ux0:/data/desktop-mode/Desktop/",NULL,".png,.jpg,.jpeg"};if(dm_file_dialog(&o,chosen,w)<0)dm_status("Selettore file gia aperto");}
static void open_view(DmWindow*w,const char*a){if(a&&*a)load(w,a);}
static void draw(DmWindow*w){Viewer*v=w->state;dm_rect(w->x+15,w->y+38,130,32,DM_COLOR(205,224,244,255));dm_text(w->x+27,w->y+61,"Apri immagine",DM_COLOR(24,38,55,255));dm_rect(w->x+15,w->y+80,w->w-30,w->h-120,DM_COLOR(42,47,54,255));if(v->image){float scale=(float)(w->w-40)/v->width;float sy=(float)(w->h-130)/v->height;if(sy<scale)scale=sy;int width=v->width*scale,height=v->height*scale;dm_image_draw(v->image,w->x+(w->w-width)/2,w->y+85+(w->h-130-height)/2,width,height);char s[120];snprintf(s,sizeof(s),"%u x %u | %.70s",v->width,v->height,strrchr(v->path,'/')?strrchr(v->path,'/')+1:v->path);dm_text(w->x+20,w->y+w->h-16,s,DM_COLOR(24,38,55,255));}else dm_text(w->x+30,w->y+120,"Apri un file PNG o JPEG",DM_COLOR(255,255,255,255));}
static void click(DmWindow*w,int x,int y){if(x>=15&&x<145&&y>=38&&y<70)open_file(w);}
static int close_view(DmWindow*w){dm_image_free(((Viewer*)w->state)->image);return 1;}
static void tick(DmWindow*w,unsigned ms){(void)ms;Viewer*v=w->state;dm_fs_path_update(v->path,sizeof(v->path),&v->revision);}
const DmApp dm_images_app={DM_API_VERSION,"images","Anteprima immagini",sizeof(Viewer),open_view,draw,click,0,0,close_view,".png,.jpg,.jpeg",tick};
DM_EXPORT_APP(dm_images_app)
