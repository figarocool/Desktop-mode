#include "../wasm_app.h"
#define DM_WIDGETS_WASM
#include "../dm_widgets.h"
#define MAX_W 960
#define MAX_H 480
static uint32_t pixels[MAX_W*MAX_H];
static char path[1024],selected[1024],input[256],launch[1024],error[256];
static int page,pages,zoom=100,offset_x,offset_y,image,vieww=640,viewh=300,dirty,opening,password_mode,page_mode;
void *memcpy(void*d,const void*s,unsigned long n){unsigned char*a=d;const unsigned char*b=s;for(unsigned long i=0;i<n;i++)a[i]=b[i];return d;}
void *memset(void*d,int v,unsigned long n){unsigned char*a=d;for(unsigned long i=0;i<n;i++)a[i]=(unsigned char)v;return d;}
uint32_t dm_app_abi_version(void){return DM_WASM_APP_ABI_VERSION;}
static int contains(const char*s,const char*q){for(uint32_t i=0;s[i];i++){uint32_t j=0;while(q[j]&&s[i+j]==q[j])j++;if(!q[j])return 1;}return 0;}
uint32_t dm_app_file_buffer(void){return(uint32_t)(uintptr_t)selected;}
uint32_t dm_app_text_buffer(void){return(uint32_t)(uintptr_t)input;}
uint32_t dm_app_argument_buffer(void){return(uint32_t)(uintptr_t)launch;}
static void render(void){if(!pages)return;int w=vieww,h=viewh;if(w>MAX_W)w=MAX_W;if(h>MAX_H)h=MAX_H;int32_t mx=0,my=0;error[0]=0;if(dm_host_pdf_render(page,zoom,offset_x,offset_y,w,h,pixels,&mx,&my,error,sizeof(error))<0){dirty=0;return;}offset_x=mx<offset_x?mx:offset_x;offset_y=my<offset_y?my:offset_y;if(image>0)dm_host_image_update(image,pixels);else image=dm_host_image_create(w,h,pixels);dirty=0;}
static int open_pdf(const char*p,const char*password){error[0]=0;int n=dm_host_pdf_open(p,password?password:"",error,sizeof(error));if(n<1){pages=0;page=0;if(error[0]&&!password&&(contains(error,"Password")||contains(error,"password")||contains(error,"encrypted"))){password_mode=1;dm_host_text_prompt("PDF protetto - password","",input,sizeof(input));}return -1;}for(uint32_t i=0;p[i]&&i<sizeof(path)-1;i++)path[i]=p[i];path[sizeof(path)-1]=0;pages=n;page=offset_x=offset_y=0;zoom=100;dirty=1;render();return 0;}
void dm_app_file_result(uint32_t n){if(n>=sizeof(selected))return;selected[n]=0;opening=0;open_pdf(selected,"");}
void dm_app_init(const char*a){pages=page=offset_x=offset_y=0;zoom=100;dirty=opening=password_mode=page_mode=0;path[0]=selected[0]=input[0]=error[0]=0;if(a&&a[0])open_pdf(a,"");}
void dm_app_text(uint32_t n){if(n>=sizeof(input))n=sizeof(input)-1;input[n]=0;if(password_mode){password_mode=0;open_pdf(path,input);input[0]=0;}else if(page_mode){page_mode=0;int value=0;for(uint32_t i=0;input[i]>='0'&&input[i]<='9';i++){value=value*10+input[i]-'0';if(value>pages)break;}if(value>0&&value<=pages){page=value-1;offset_x=offset_y=0;dirty=1;}input[0]=0;}}
static void change_page(int d){if(!pages)return;int next=page+d;if(next<0)next=0;if(next>=pages)next=pages-1;if(next!=page){page=next;offset_x=offset_y=0;dirty=1;}}
void dm_app_key(int32_t k){if(k==DM_WASM_KEY_LEFT)change_page(-1);if(k==DM_WASM_KEY_RIGHT)change_page(1);if(k==DM_WASM_KEY_UP&&zoom>100){offset_y-=80;if(offset_y<0)offset_y=0;dirty=1;}if(k==DM_WASM_KEY_DOWN&&zoom>100){offset_y+=80;dirty=1;}if(k==DM_WASM_KEY_HOME)change_page(-page);if(k==DM_WASM_KEY_END)change_page(pages-1-page);}
void dm_app_draw(int32_t w,int32_t h){const char*buttons[]={"Apri","Indietro","Avanti","Zoom -","Zoom +","Adatta"};for(int i=0;i<6;i++)dmw_button((DmWidgetRect){8+i*94,3,88,29},buttons[i],0,0);vieww=w-24;if(vieww>MAX_W)vieww=MAX_W;viewh=h-90;if(viewh>MAX_H)viewh=MAX_H;if(vieww<1)vieww=1;if(viewh<1)viewh=1;if(dirty)render();dm_host_rect(12,38,vieww,viewh,0xFF494D53u);if(image>0)dm_host_image_draw(image,12,39,vieww,viewh);else dm_host_text(24,67,error[0]?error:"Apri un documento PDF",0xFFFFFFFFu);char status[160]="Nessun documento";if(pages){uint32_t n=0;const char*prefix="Pagina ";while(*prefix)status[n++]=*prefix++;int v=page+1,d[8],k=0;do{d[k++]=v%10;v/=10;}while(v&&k<8);while(k)status[n++]=(char)('0'+d[--k]);status[n++]='/';v=pages;k=0;do{d[k++]=v%10;v/=10;}while(v&&k<8);while(k)status[n++]=(char)('0'+d[--k]);status[n++]=' ';status[n++]='|';status[n++]=' ';status[n++]='Z';status[n++]='o';status[n++]='o';status[n++]='m';status[n++]=' ';v=zoom;k=0;do{d[k++]=v%10;v/=10;}while(v&&k<8);while(k)status[n++]=(char)('0'+d[--k]);status[n++]='%';status[n]=0;}dm_host_text(12,h-21,status,0xFF182637u);}
void dm_app_click(int32_t x,int32_t y,uint32_t b){if(!b)return;if(pages&&y>=viewh-30){page_mode=1;dm_host_text_prompt("Vai a pagina","",input,sizeof(input));return;}if(y<3||y>=34||x<8||x>=572)return;int button=-1;for(int i=0;i<6;i++)if(dmw_button_click((DmWidgetRect){8+i*94,3,88,29},x,y,0)){button=i;break;}if(button==0){opening=1;dm_host_file_dialog("Apri documento PDF",".pdf",0,selected,sizeof(selected));}else if(button==1)change_page(-1);else if(button==2)change_page(1);else if(button==3&&zoom>100){zoom-=25;dirty=1;}else if(button==4&&zoom<400){zoom+=25;dirty=1;}else if(button==5){zoom=100;offset_x=offset_y=0;dirty=1;}}
void dm_app_menu(int32_t command){if(command==DM_WASM_MENU_OPEN){opening=1;dm_host_file_dialog("Apri documento PDF",".pdf",0,selected,sizeof(selected));}else if(command==DM_WASM_MENU_PREVIOUS)change_page(-1);else if(command==DM_WASM_MENU_NEXT)change_page(1);else if(command==DM_WASM_MENU_ZOOM_IN&&zoom<400){zoom+=25;dirty=1;}else if(command==DM_WASM_MENU_ZOOM_OUT&&zoom>100){zoom-=25;dirty=1;}}
void dm_app_tick(uint32_t ms){(void)ms;if(dirty)render();}
void dm_app_close(void){if(image>0)dm_host_image_free(image);image=0;dm_host_pdf_close();}
