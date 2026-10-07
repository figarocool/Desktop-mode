#include "../wasm_app.h"
#define DM_WIDGETS_WASM
#include "../dm_widgets.h"

static int32_t selected=-1;
static char title[96];
static int32_t button_y,button_x[3],button_w[3],shown_count,row_width,memory_x,status_x,title_max_width;
static void shorten_title(void){int32_t n=0,truncated=0;while(title[n])n++;while(n>3&&dm_host_text_width(title)>title_max_width){do{n--;}while(n>0&&((unsigned char)title[n]&0xC0)==0x80);title[n]=0;truncated=1;}if(truncated&&n>3){title[n-3]='.';title[n-2]='.';title[n-1]='.';}}
uint32_t dm_app_abi_version(void){return DM_WASM_APP_ABI_VERSION;}
void dm_app_init(const char *argument){(void)argument;selected=-1;}
void dm_app_draw(int32_t width,int32_t height){
    int32_t count=dm_host_task_count();if(count<0)count=0;if(count>8)count=8;
    button_y=height-48;shown_count=(button_y-50)/30;if(shown_count<0)shown_count=0;if(count>shown_count)count=shown_count;row_width=width-40;if(row_width<100)row_width=100;status_x=width-130;memory_x=width-270;if(memory_x<180)memory_x=180;title_max_width=memory_x-42;if(title_max_width<24)title_max_width=24;
    dm_host_text(20,28,"Applicazioni | memoria usata",DM_WASM_COLOR_TEXT);
    for(int32_t i=0;i<count;i++){
        int32_t minimized=dm_host_task_get(i,title,sizeof(title));shorten_title();
        dm_host_rect(20,45+i*30,row_width,28,i==selected?DM_WASM_COLOR_HIGHLIGHT:0xFFFFFFFFu);
        dm_host_text(30,65+i*30,title,DM_WASM_COLOR_TEXT);
        int32_t kb=dm_host_task_memory(i);char memory[24];
        if(kb<0)kb=0;
        unsigned v=(unsigned)kb;char rev[12];unsigned n=0;do{rev[n++]=(char)('0'+v%10);v/=10;}while(v&&n<sizeof(rev));
        unsigned out=0;while(n)memory[out++]=rev[--n];memory[out++]=' ';memory[out++]='K';memory[out++]='i';memory[out++]='B';memory[out]=0;
        dm_host_text(memory_x,65+i*30,memory,DM_WASM_COLOR_TEXT);
        dm_host_text(status_x,65+i*30,minimized==1?"Ridotta":"Aperta",DM_WASM_COLOR_TEXT);
    }
    const char*labels[]={"Mostra","Minimizza","Chiudi app"};int gap=10,bw=(width-40-2*gap)/3;if(bw<92)bw=92;for(int i=0;i<3;i++){button_x[i]=20+i*(bw+gap);button_w[i]=bw;}
    for(int i=0;i<3;i++)dmw_button((DmWidgetRect){button_x[i],button_y,button_w[i],34},labels[i],0,0);
}
void dm_app_click(int32_t x,int32_t y,uint32_t buttons){
    if(!buttons)return;
    int32_t count=dm_host_task_count();if(count>shown_count)count=shown_count;
    if(y>=45&&y<45+count*30){selected=(y-45)/30;return;}
    if(selected<0||selected>=count)return;
    if(y>=button_y&&y<button_y+34){for(int i=0;i<3;i++)if(dmw_button_click((DmWidgetRect){button_x[i],button_y,button_w[i],34},x,y,0))dm_host_task_action(selected,i);}
}
void dm_app_menu(int32_t command){(void)command;}
void dm_app_close(void){}
