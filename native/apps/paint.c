#include "../app-sdk/desktop_plugin.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#define PW 560
#define PH 240
#define WHITE DM_COLOR(255,255,255,255)
typedef struct{uint32_t *pixels,*undo;void*image;uint32_t color;int brush,eraser,dirty,dragging,lastx,lasty,has_undo,force;char path[DM_PATH_MAX];uint64_t revision;} Paint;
static const uint32_t colors[]={DM_COLOR(0,0,0,255),WHITE,DM_COLOR(220,40,40,255),DM_COLOR(245,180,20,255),DM_COLOR(30,165,70,255),DM_COLOR(25,95,220,255),DM_COLOR(155,50,180,255),DM_COLOR(130,75,40,255)};
static void snapshot(Paint*p){memcpy(p->undo,p->pixels,PW*PH*sizeof(uint32_t));p->has_undo=1;}
static void clear(Paint*p){for(int i=0;i<PW*PH;i++)p->pixels[i]=WHITE;dm_image_update(p->image,p->pixels);}
static void open_paint(DmWindow*w,const char*a){(void)a;Paint*p=w->state;dm_fs_path_update(p->path,sizeof(p->path),&p->revision);p->color=colors[0];p->brush=3;p->pixels=malloc(PW*PH*4);p->undo=malloc(PW*PH*4);if(!p->pixels||!p->undo){free(p->pixels);free(p->undo);p->pixels=p->undo=NULL;dm_status("Memoria insufficiente per Paint");return;}clear(p);p->image=dm_image_create(PW,PH,p->pixels);if(!p->image)dm_status("Memoria insufficiente per la tela");}
static void save_result(const char*path,int replace,void*ctx){DmWindow*w=ctx;if(!w->used)return;Paint*p=w->state;if(!p->pixels)return;const char*ext=strrchr(path,'.');if(!ext||strcasecmp(ext,".png")){dm_status("Paint salva in PNG: usa un nome con estensione .png");return;}if(dm_image_save_png(path,PW,PH,p->pixels,!replace)<0){dm_status("Salvataggio PNG fallito");return;}dm_fs_path_update(p->path,sizeof(p->path),&p->revision);snprintf(p->path,sizeof(p->path),"%s",path);p->dirty=0;dm_status("Disegno salvato");}
static void save_as(DmWindow*w){Paint*p=w->state;dm_fs_path_update(p->path,sizeof(p->path),&p->revision);char dir[DM_PATH_MAX];snprintf(dir,sizeof(dir),"%s",p->path[0]?p->path:"ux0:/data/desktop-mode/Desktop/");if(p->path[0])dm_fs_parent(dir);const char*name=strrchr(p->path,'/');DmFileDialogOptions o={DM_FILE_SAVE,"Paint - Salva PNG",dir,name?name+1:"Disegno.png",".png"};if(dm_file_dialog(&o,save_result,w)<0)dm_status("Selettore file gia aperto");}
static void saved(DmWindow*w){Paint*p=w->state;dm_fs_path_update(p->path,sizeof(p->path),&p->revision);if(!p->path[0])save_as(w);else {char path[DM_PATH_MAX];snprintf(path,sizeof(path),"%s",p->path);save_result(path,1,w);}}
static void dot(Paint*p,int x,int y){int radius=p->eraser?8:p->brush;for(int dy=-radius;dy<=radius;dy++)for(int dx=-radius;dx<=radius;dx++)if(dx*dx+dy*dy<=radius*radius&&x+dx>=0&&x+dx<PW&&y+dy>=0&&y+dy<PH)p->pixels[(y+dy)*PW+x+dx]=p->eraser?WHITE:p->color;}
static void stroke(Paint*p,int x,int y){int dx=x-p->lastx,dy=y-p->lasty,steps=dx<0?-dx:dx;int sy=dy<0?-dy:dy;if(sy>steps)steps=sy;if(!steps)dot(p,x,y);else for(int i=0;i<=steps;i++)dot(p,p->lastx+dx*i/steps,p->lasty+dy*i/steps);p->lastx=x;p->lasty=y;p->dirty=1;dm_image_update(p->image,p->pixels);}
static void new_result(int yes,void*ctx){DmWindow*w=ctx;if(yes&&w->used){Paint*p=w->state;snapshot(p);clear(p);p->dirty=1;p->path[0]=0;}}
static int canvas_point(DmWindow*w,int x,int y,int*px,int*py){
 int width=w->w-40,height=w->h-155;if(width<1||height<1||x<20||x>=20+width||y<120||y>=120+height)return 0;
 *px=(x-20)*PW/width;*py=(y-120)*PH/height;return 1;
}
static void draw(DmWindow*w){Paint*p=w->state;const char*b[]={"Nuovo","Salva","Salva come","Annulla","Gomma"};for(int i=0;i<5;i++){dm_rect(w->x+15+i*130,w->y+36,120,30,DM_COLOR(209,226,244,255));dm_text(w->x+23+i*130,w->y+57,b[i],DM_COLOR(24,38,55,255));}for(int i=0;i<8;i++){dm_rect(w->x+20+i*37,w->y+77,29,29,colors[i]);if(p->color==colors[i]&&!p->eraser)dm_rect(w->x+20+i*37,w->y+108,29,3,colors[0]);}dm_text(w->x+330,w->y+99,dm_localize(p->eraser?"Gomma attiva":"Pennello: clic per cambiare",p->eraser?"Eraser active":"Brush: click to change",p->eraser?"Goma activa":"Pincel: pulsa para cambiar"),colors[0]);dm_rect(w->x+19,w->y+119,w->w-38,w->h-153,colors[0]);dm_image_draw(p->image,w->x+20,w->y+120,w->w-40,w->h-155);char s[100];snprintf(s,sizeof(s),dm_localize("Tela %d x %d | pennello %d | %s","Canvas %d x %d | brush %d | %s","Lienzo %d x %d | pincel %d | %s"),PW,PH,p->brush,dm_localize(p->dirty?"Modificato":"Salvato",p->dirty?"Modified":"Saved",p->dirty?"Modificado":"Guardado"));dm_text_raw(w->x+20,w->y+w->h-12,s,colors[0]);}
static void click(DmWindow*w,int x,int y){Paint*p=w->state;if(!p->pixels)return;if(y>=36&&y<66&&x>=15&&x<665){int b=(x-15)/130;if(b==0){if(p->dirty)dm_confirm("Nuovo disegno","Scartare il disegno attuale?",new_result,w);else new_result(1,w);}if(b==1)saved(w);if(b==2)save_as(w);if(b==3&&p->has_undo){uint32_t t;for(int i=0;i<PW*PH;i++){t=p->pixels[i];p->pixels[i]=p->undo[i];p->undo[i]=t;}p->dirty=1;dm_image_update(p->image,p->pixels);}if(b==4)p->eraser=!p->eraser;return;}if(y>=77&&y<107&&x>=20&&x<316){int i=(x-20)/37;p->color=colors[i];p->eraser=0;return;}if(y>=77&&y<107&&x>=330){p->brush=p->brush==3?6:p->brush==6?1:3;return;}int cx,cy;if(canvas_point(w,x,y,&cx,&cy)){snapshot(p);p->lastx=cx;p->lasty=cy;stroke(p,cx,cy);p->dragging=1;}}
static void tick(DmWindow*w,unsigned ms){(void)ms;Paint*p=w->state;if(!p->pixels)return;dm_fs_path_update(p->path,sizeof(p->path),&p->revision);DmPointerState q;dm_pointer_state(w,&q);if(!q.held||!q.focused){p->dragging=0;return;}int cx,cy;if(p->dragging&&canvas_point(w,q.x,q.y,&cx,&cy))stroke(p,cx,cy);}
static void close_result(int yes,void*ctx){DmWindow*w=ctx;if(yes&&w->used){((Paint*)w->state)->force=1;dm_close(w);}}
static int close_paint(DmWindow*w){Paint*p=w->state;if(p->dirty&&!p->force){dm_confirm("Chiudi Paint","Chiudere senza salvare il disegno?",close_result,w);return 0;}dm_image_free(p->image);free(p->pixels);free(p->undo);return 1;}
static void key(DmWindow*w,int k){if(k==DM_KEY_SAVE)saved(w);}
const DmApp dm_paint_app={DM_API_VERSION,"paint","Paint",sizeof(Paint),open_paint,draw,click,0,key,close_paint,"",tick};
DM_EXPORT_APP(dm_paint_app)
