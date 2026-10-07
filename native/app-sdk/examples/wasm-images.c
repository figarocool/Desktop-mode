#include "../wasm_app.h"
#define DM_WIDGETS_WASM
#include "../dm_widgets.h"

static char launch_path[1024],selected_path[1024];
static int32_t image_handle;
static uint32_t image_width,image_height;
static int fit,offset_x,offset_y,drag_axis,drag_grab;
static int vx,vy,vw,vh,content_w,content_h,hbar,vbar,view_width;
uint32_t dm_app_abi_version(void){return DM_WASM_APP_ABI_VERSION;}
uint32_t dm_app_argument_buffer(void){return (uint32_t)(uintptr_t)launch_path;}
uint32_t dm_app_file_buffer(void){return (uint32_t)(uintptr_t)selected_path;}
static void load_selected(void){int32_t next=dm_host_image_load(selected_path);if(next>0){if(image_handle>0)dm_host_image_free(image_handle);image_handle=next;dm_host_image_size(image_handle,&image_width,&image_height);fit=0;offset_x=offset_y=0;}}
void dm_app_file_result(uint32_t n){if(n>=sizeof(selected_path))return;selected_path[n]=0;load_selected();}
void dm_app_init(const char *argument){image_handle=0;image_width=image_height=0;fit=0;offset_x=offset_y=drag_axis=0;if(argument&&argument[0]){for(uint32_t i=0;argument[i]&&i<sizeof(launch_path)-1;i++)launch_path[i]=argument[i];launch_path[sizeof(launch_path)-1]=0;image_handle=dm_host_image_load(argument);if(image_handle>0)dm_host_image_size(image_handle,&image_width,&image_height);}}
static int clamp(int v,int lo,int hi){return v<lo?lo:v>hi?hi:v;}
static uint32_t number(char*out,uint32_t n){char tmp[12];uint32_t k=0;do{tmp[k++]=(char)('0'+n%10);n/=10;}while(n&&k<sizeof(tmp));for(uint32_t i=0;i<k;i++)out[i]=tmp[k-i-1];return k;}
static void layout(int32_t width,int32_t height){
    vx=8;vy=42;vw=width-16;vh=height-76;if(vw<1)vw=1;if(vh<1)vh=1;
    content_w=(int)image_width;content_h=(int)image_height;
    hbar=vbar=0;
    if(!fit&&image_width&&image_height){
        int basew=vw,baseh=vh;
        for(int i=0;i<3;i++){int aw=basew-(vbar?13:0),ah=baseh-(hbar?13:0);hbar=content_w>aw;vbar=content_h>ah;}
        vw=basew-(vbar?13:0);vh=baseh-(hbar?13:0);
    }
    if(fit&&image_width&&image_height){double sx=(double)vw/image_width,sy=(double)vh/image_height,s=sx<sy?sx:sy;content_w=(int)(image_width*s);content_h=(int)(image_height*s);if(content_w<1)content_w=1;if(content_h<1)content_h=1;offset_x=offset_y=0;}
    offset_x=clamp(offset_x,0,content_w-vw);offset_y=clamp(offset_y,0,content_h-vh);
}
void dm_app_draw(int32_t width,int32_t height){
    view_width=width;
    dm_host_text(12,24,"Anteprima immagini | PNG / JPEG",DM_WASM_COLOR_TEXT);
    dmw_button((DmWidgetRect){width-192,4,84,28},"Adatta",0,fit);
    dmw_button((DmWidgetRect){width-98,4,84,28},"100%",0,!fit);
    layout(width,height);dm_host_rect(vx,vy,vw,vh,0xFF343A42u);
    if(image_handle>0&&image_width&&image_height){int ix=vx+(content_w<vw?(vw-content_w)/2:0)-offset_x;int iy=vy+(content_h<vh?(vh-content_h)/2:0)-offset_y;dm_host_image_draw_clipped(image_handle,ix,iy,content_w,content_h,vx,vy,vw,vh);}
    else dm_host_text(vx+18,vy+35,"Apri un PNG o JPEG da Risorse del computer",0xFFFFFFFFu);
    if(hbar)dm_host_scrollbar_draw(vx,vy+vh+3,vw,10,0,content_w,vw,offset_x);
    if(vbar)dm_host_scrollbar_draw(vx+vw+3,vy,10,vh,1,content_h,vh,offset_y);
    if(hbar&&vbar)dm_host_rect(vx+vw+3,vy+vh+3,10,10,0xFFD0D4D8u);
    dm_host_rect(8,height-30,width-16,24,0xFFE6E9EDu);
    if(image_width&&image_height){char status[48]="Immagine: ";uint32_t n=10;n+=number(status+n,image_width);status[n++]=' ';status[n++]='x';status[n++]=' ';n+=number(status+n,image_height);status[n]=0;dm_host_text(14,height-12,status,DM_WASM_COLOR_TEXT);}
}
static int thumb_size(int view,int content){int n=content>0?view*view/content:view;if(n<20)n=20;if(n>view)n=view;return n;}
static void scroll_at(int axis,int p){int view=axis==1?vh:vw,content=axis==1?content_h:content_w;if(content<=view)return;int thumb=thumb_size(view,content),travel=view-thumb;if(travel<1)return;int local=p-(axis==1?vy:vx);int pos=clamp(local-thumb/2,0,travel);int off=pos*(content-view)/travel;if(axis==1)offset_y=off;else offset_x=off;}
void dm_app_click(int32_t x,int32_t y,uint32_t buttons){if(!buttons)return;if(y<34&&x>=0){if(dmw_button_click((DmWidgetRect){view_width-98,4,84,28},x,y,0)){fit=0;offset_x=offset_y=0;}else if(dmw_button_click((DmWidgetRect){view_width-192,4,84,28},x,y,0)){fit=1;offset_x=offset_y=0;}return;}if(!fit&&vbar&&x>=vx+vw+2&&y>=vy&&y<vy+vh){scroll_at(1,y);int thumb=thumb_size(vh,content_h),travel=vh-thumb;drag_axis=1;drag_grab=y-vy-(travel>0?offset_y*travel/(content_h-vh):0);return;}if(!fit&&hbar&&y>=vy+vh+2&&x>=vx&&x<vx+vw){scroll_at(0,x);int thumb=thumb_size(vw,content_w),travel=vw-thumb;drag_axis=2;drag_grab=x-vx-(travel>0?offset_x*travel/(content_w-vw):0);return;}drag_axis=0;}
void dm_app_tick(uint32_t ms){(void)ms;int32_t x=0,y=0,held=0;if(!dm_host_pointer_state(&x,&y,&held)||!held){drag_axis=0;return;}if(drag_axis==1){int travel=vh-thumb_size(vh,content_h);if(travel>0&&content_h>vh)offset_y=clamp((y-vy-drag_grab)*(content_h-vh)/travel,0,content_h-vh);}else if(drag_axis==2){int travel=vw-thumb_size(vw,content_w);if(travel>0&&content_w>vw)offset_x=clamp((x-vx-drag_grab)*(content_w-vw)/travel,0,content_w-vw);}}
void dm_app_key(int32_t key){if(key==DM_WASM_KEY_LEFT)offset_x-=40;else if(key==DM_WASM_KEY_RIGHT)offset_x+=40;else if(key==DM_WASM_KEY_UP||key==DM_WASM_KEY_SCROLL_UP)offset_y-=40;else if(key==DM_WASM_KEY_DOWN||key==DM_WASM_KEY_SCROLL_DOWN)offset_y+=40;}
void dm_app_menu(int32_t command){if(command==DM_WASM_MENU_OPEN)dm_host_file_dialog("Apri immagine",".png,.jpg,.jpeg",0,selected_path,sizeof(selected_path));}
void dm_app_close(void){if(image_handle>0)dm_host_image_free(image_handle);image_handle=0;}
