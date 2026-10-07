#define DM_PLUGIN_HEAP_SIZE (96*1024*1024)
#include "../app-sdk/desktop_plugin.h"
#include "pdf_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define INK DM_COLOR(24,38,55,255)
typedef struct {
 PdfEngine*engine;unsigned char*data;void*image;
 char path[DM_PATH_MAX],pending[DM_PATH_MAX],error[180];uint64_t revision;
 int pages,page,zoom,offset_x,offset_y,max_x,max_y,render_width,render_height,dirty;
} Pdf;
static void viewport(DmWindow*w,int*width,int*height){*width=w->w-30;*height=w->h-135;}
static void*render(PdfEngine*engine,DmWindow*w,int page,int zoom,int ox,int oy,int*maxx,int*maxy,char*error,size_t capacity){
 int width,height;viewport(w,&width,&height);if(width<1||height<1||width>960||height>544)return NULL;
 uint32_t*pixels=malloc((size_t)width*height*4);if(!pixels){snprintf(error,capacity,"Memoria insufficiente");return NULL;}
 void*image=NULL;if(!pdf_engine_render(engine,page,zoom,ox,oy,width,height,pixels,maxx,maxy,error,capacity))image=dm_image_create(width,height,pixels);
 free(pixels);if(!image&&!error[0])snprintf(error,capacity,"Memoria insufficiente per la pagina");return image;
}
static void load_pdf(DmWindow*,const char*,const char*);
static void password_entered(const char*password,void*ctx){DmWindow*w=ctx;if(w->used){char path[DM_PATH_MAX];snprintf(path,sizeof(path),"%s",((Pdf*)w->state)->pending);load_pdf(w,path,password);}}
static void load_pdf(DmWindow*w,const char*path,const char*password){
 Pdf*p=w->state;size_t capacity=512*1024+1;unsigned char*data=NULL;int length=-1;p->error[0]=0;
 for(;;){data=malloc(capacity);if(!data){snprintf(p->error,sizeof(p->error),"Memoria insufficiente");break;}length=dm_fs_read(path,(char*)data,capacity);if(length!=-2)break;free(data);data=NULL;if(capacity>=16*1024*1024+1){snprintf(p->error,sizeof(p->error),"PDF oltre il limite di 16 MiB");break;}capacity=(capacity-1)*2+1;}
 if(!data||length<0){free(data);if(!p->error[0])snprintf(p->error,sizeof(p->error),"PDF non accessibile");dm_status(p->error);return;}
 PdfEngine*engine=pdf_engine_open(data,length,password,p->error,sizeof(p->error));
 if(!engine){free(data);dm_status(p->error);if(!password&&(strstr(p->error,"Password")||strstr(p->error,"password")||strstr(p->error,"encrypted"))){snprintf(p->pending,sizeof(p->pending),"%s",path);dm_prompt("Password PDF","",password_entered,w);}return;}
 int pages=pdf_engine_pages(engine),mx=0,my=0;void*image=pages>0?render(engine,w,0,100,0,0,&mx,&my,p->error,sizeof(p->error)):NULL;
 if(!image){pdf_engine_close(engine);free(data);if(!p->error[0])snprintf(p->error,sizeof(p->error),"PDF senza pagine leggibili");dm_status(p->error);return;}
 dm_image_free(p->image);pdf_engine_close(p->engine);free(p->data);p->engine=engine;p->data=data;p->image=image;p->pages=pages;p->page=0;p->zoom=100;p->offset_x=p->offset_y=0;p->max_x=mx;p->max_y=my;p->dirty=0;viewport(w,&p->render_width,&p->render_height);dm_fs_path_update(p->path,sizeof(p->path),&p->revision);snprintf(p->path,sizeof(p->path),"%s",path);p->error[0]=0;
}
static void chosen(const char*path,int replace,void*ctx){(void)replace;DmWindow*w=ctx;if(w->used)load_pdf(w,path,NULL);}
static void choose(DmWindow*w){DmFileDialogOptions o={DM_FILE_OPEN,"Apri PDF","ux0:/data/desktop-mode/Desktop/",NULL,".pdf"};if(dm_file_dialog(&o,chosen,w)<0)dm_status("Selettore file gia aperto");}
static void open_pdf(DmWindow*w,const char*a){Pdf*p=w->state;p->zoom=100;dm_fs_path_update(p->path,sizeof(p->path),&p->revision);if(a&&*a)load_pdf(w,a,NULL);}
static void change_page(Pdf*p,int page){if(page<0||page>=p->pages||page==p->page)return;p->page=page;p->offset_x=p->offset_y=0;p->dirty=1;}
static void page_entered(const char*text,void*ctx){DmWindow*w=ctx;if(!w->used)return;Pdf*p=w->state;char*end;long page=strtol(text,&end,10);while(isspace((unsigned char)*end))end++;if(text==end||*end||page<1||page>p->pages){dm_status("Numero pagina non valido");return;}change_page(p,page-1);}
static void zoom(Pdf*p,int delta){int next=p->zoom+delta;if(next<100)next=100;if(next>400)next=400;if(next!=p->zoom){p->zoom=next;p->offset_x=p->offset_y=0;p->dirty=1;}}
static void pan(Pdf*p,int dx,int dy){int x=p->offset_x+dx,y=p->offset_y+dy;if(x<0)x=0;if(y<0)y=0;if(x>p->max_x)x=p->max_x;if(y>p->max_y)y=p->max_y;if(x!=p->offset_x||y!=p->offset_y){p->offset_x=x;p->offset_y=y;p->dirty=1;}}
static void key(DmWindow*w,int k){Pdf*p=w->state;if(!p->engine)return;if(k==DM_KEY_LEFT){if(p->zoom>100)pan(p,-80,0);else change_page(p,p->page-1);}if(k==DM_KEY_RIGHT){if(p->zoom>100)pan(p,80,0);else change_page(p,p->page+1);}if(k==DM_KEY_UP||k==DM_KEY_SCROLL_UP){if(p->zoom>100)pan(p,0,-80);else change_page(p,p->page-1);}if(k==DM_KEY_DOWN||k==DM_KEY_SCROLL_DOWN){if(p->zoom>100)pan(p,0,80);else change_page(p,p->page+1);}if(k==DM_KEY_HOME)change_page(p,0);if(k==DM_KEY_END)change_page(p,p->pages-1);}
static void click(DmWindow*w,int x,int y){Pdf*p=w->state;if(y>=38&&y<70&&x>=15&&x<645){int b=(x-15)/90;if(b==0){choose(w);return;}if(!p->engine)return;if(b==1)change_page(p,p->page-1);if(b==2)change_page(p,p->page+1);if(b==3){char value[32];snprintf(value,sizeof(value),"%d",p->page+1);dm_prompt("Vai alla pagina",value,page_entered,w);}if(b==4)zoom(p,-25);if(b==5)zoom(p,25);if(b==6){p->zoom=100;p->offset_x=p->offset_y=0;p->dirty=1;}return;}if(p->engine&&y>=w->h-45&&y<w->h-15&&x>=15&&x<315){switch((x-15)/75){case 0:pan(p,-80,0);break;case 1:pan(p,80,0);break;case 2:pan(p,0,-80);break;case 3:pan(p,0,80);break;}}}
static void tick(DmWindow*w,unsigned ms){(void)ms;Pdf*p=w->state;dm_fs_path_update(p->path,sizeof(p->path),&p->revision);if(!p->engine)return;int width,height;viewport(w,&width,&height);if(width!=p->render_width||height!=p->render_height){p->dirty=1;p->offset_x=p->offset_y=0;}if(!p->dirty)return;p->error[0]=0;int mx,my;void*image=render(p->engine,w,p->page,p->zoom,p->offset_x,p->offset_y,&mx,&my,p->error,sizeof(p->error));p->dirty=0;p->render_width=width;p->render_height=height;if(image){dm_image_free(p->image);p->image=image;p->max_x=mx;p->max_y=my;if(p->offset_x>mx)p->offset_x=mx;if(p->offset_y>my)p->offset_y=my;}else {dm_image_free(p->image);p->image=NULL;dm_status(p->error);}}
static void draw(DmWindow*w){Pdf*p=w->state;const char*buttons[]={"Apri","Indietro","Avanti","Pagina","Zoom -","Zoom +","Adatta"};for(int i=0;i<7;i++){dm_rect(w->x+15+i*90,w->y+38,84,32,DM_COLOR(208,226,244,255));dm_text(w->x+20+i*90,w->y+60,buttons[i],INK);}int width,height;viewport(w,&width,&height);dm_rect(w->x+15,w->y+80,width,height,DM_COLOR(60,65,72,255));if(p->image)dm_image_draw(p->image,w->x+15,w->y+80,width,height);else dm_text(w->x+35,w->y+125,p->error[0]?p->error:"Apri un documento PDF",DM_COLOR(255,255,255,255));const char*pan_labels[]={"<",">","Su","Giu"};for(int i=0;i<4;i++){dm_rect(w->x+15+i*75,w->y+w->h-45,65,30,DM_COLOR(208,226,244,255));dm_text_center(w->x+47+i*75,w->y+w->h-24,pan_labels[i],INK);}char status[200];snprintf(status,sizeof(status),dm_localize("Pagina %d/%d | Zoom %d%% | %.28s","Page %d/%d | Zoom %d%% | %.28s","Pagina %d/%d | Zoom %d%% | %.28s"),p->engine?p->page+1:0,p->pages,p->zoom,p->error[0]?p->error:strrchr(p->path,'/')?strrchr(p->path,'/')+1:"");dm_text_raw(w->x+325,w->y+w->h-24,status,INK);}
static int close_pdf(DmWindow*w){Pdf*p=w->state;dm_image_free(p->image);pdf_engine_close(p->engine);free(p->data);return 1;}
const DmApp dm_pdf_app={DM_API_VERSION,"pdf","Visualizzatore PDF",sizeof(Pdf),open_pdf,draw,click,0,key,close_pdf,".pdf",tick};
DM_EXPORT_APP(dm_pdf_app)
