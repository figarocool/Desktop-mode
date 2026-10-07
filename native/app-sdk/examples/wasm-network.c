#include "../wasm_app.h"
#define DM_WIDGETS_WASM
#include "../dm_widgets.h"

static DmNetworkInfo state;
static DmNetworkRow row;
static uint32_t elapsed_ms,last_click_index=UINT32_MAX;
static uint64_t last_click_at;
static int view_width,view_height;
static const uint32_t ink=0xFF182637u,button=0xFFE0EAF4u,selected=0xFF97C3F6u;

uint32_t dm_app_abi_version(void){return DM_WASM_APP_ABI_VERSION;}
void dm_app_init(const char *argument){(void)argument;elapsed_ms=0;last_click_index=UINT32_MAX;dm_host_network_info(&state,sizeof(state));}
void dm_app_tick(uint32_t ms){elapsed_ms+=ms;if(dm_host_network_info(&state,sizeof(state))<0){state.message[0]=0;}}
static void label(int x,int y,const char *text,uint32_t color){dm_host_text(x,y,text,color);}
void dm_app_draw(int32_t width,int32_t height){
    view_width=width;view_height=height;
    static const char *actions[]={"Scans.","Connetti","Utente","Su","Scarica","Invia","Elimina","Rinomina","Cartella","Stop"};
    int bw=(width-20)/10;if(bw>69)bw=69;if(bw<42)bw=42;
    for(int i=0;i<10;i++){int x=4+i*bw;dmw_button((DmWidgetRect){x,2,bw-2,31},actions[i],0,0);}
    char address[180];
    if(state.mode) {
        if(state.path[0]){
            int n=0;while(state.path[n]&&n<110){address[n]=state.path[n];n++;}address[n]=0;
            label(12,53,address,ink);
        }else{
            int n=0;const char *parts[]={state.server," / ",state.share};for(int p=0;p<3;p++)for(int i=0;parts[p][i]&&n<160;i++)address[n++]=parts[p][i];address[n]=0;label(12,53,address,ink);
        }
    }else label(12,53,"Dispositivi IPv4 rilevati (non tutti sono PC)",ink);
    int top=64,line_h=25,visible=(height-116)/line_h;if(visible<1)visible=1;
    for(int i=0;i<visible&&state.offset+(uint32_t)i<state.count;i++){
        int index=(int)state.offset+i,y=top+i*line_h;
        if(dm_host_network_row(index,&row,sizeof(row))<0)continue;
        
        if(state.mode==0){
            for(int k=0;row.name[k]&&k<165;k++)address[k]=row.name[k];address[165]=0;
            int n=0;while(address[n]&&n<165)n++;if(row.smb){const char *s="  [SMB 445]";for(int j=0;s[j]&&n<(int)sizeof(address)-1;j++)address[n++]=s[j];address[n]=0;}
        }else{
            const char *prefix=row.directory?"[Cartella] ":"[File] ";int n=0;for(int j=0;prefix[j];j++)address[n++]=prefix[j];
            for(int j=0;row.name[j]&&n<120;j++)address[n++]=row.name[j];
            if(!row.directory&&n<130){address[n++]=' ';address[n++]='(';uint64_t z=row.size;char digits[24];int d=0;do{digits[d++]='0'+z%10;z/=10;}while(z&&d<20);while(d&&n<150)address[n++]=digits[--d];const char *suffix=" B)";for(int j=0;suffix[j];j++)address[n++]=suffix[j];}
            address[n]=0;
        }
        DmWidgetColumn col={"",width-28};const char*cells[]={address};dmw_listview_row((DmWidgetRect){8,y-16,width-16,line_h},&col,cells,1,state.selected==(uint32_t)index,i);
    }
    if(state.scanning){int barw=width-28;if(barw<1)barw=1;dm_host_rect(14,height-46,barw,5,button);uint64_t total=state.scan_total?state.scan_total:1;uint64_t done=state.tested>total?total:state.tested;dm_host_rect(14,height-46,(int)(barw*done/total),5,0xFF3789DCu);}
    if(state.download_active||state.upload_active){char progress[96];uint64_t done=state.download_active?state.downloaded:state.uploaded,total=state.download_active?state.download_size:state.upload_size;int n=0;const char *title=state.download_active?"Scaricamento ":"Invio SMB ";while(title[n]){progress[n]=title[n];n++;}char digits[24];int d=0;do{digits[d++]='0'+done%10;done/=10;}while(done&&d<20);while(d)progress[n++]=digits[--d];progress[n++]='/';d=0;do{digits[d++]='0'+total%10;total/=10;}while(total&&d<20);while(d)progress[n++]=digits[--d];const char*s=" byte";for(int j=0;s[j];j++)progress[n++]=s[j];progress[n]=0;label(14,height-31,progress,ink);}
    label(14,height-8,state.message,ink);
}
void dm_app_click(int32_t x,int32_t y,uint32_t buttons){
    if(!buttons)return;
    int bw=(view_width-20)/10;if(bw>69)bw=69;if(bw<42)bw=42;
    if(y<35){for(int action=0;action<10;action++)if(dmw_button_click((DmWidgetRect){4+action*bw,2,bw-2,31},x,y,0)){dm_host_network_action(action);return;}}
    int top=64,line_h=25,visible=(view_height-116)/line_h;if(visible<1)visible=1;
    if(y<top-5||y>=top+visible*line_h)return;
    int index=(int)state.offset+(y-top)/line_h;if(index<0||(uint32_t)index>=state.count)return;
    int activate=last_click_index==(uint32_t)index&&elapsed_ms-last_click_at<=450;
    last_click_index=(uint32_t)index;last_click_at=elapsed_ms;
    dm_host_network_select(index,activate);
}
void dm_app_key(int32_t key){
    if(state.selected==UINT32_MAX){if(key==DM_WASM_KEY_ENTER&&state.count)dm_host_network_select(0,1);return;}
    if(key==DM_WASM_KEY_UP&&state.selected>0)dm_host_network_select((int)state.selected-1,0);
    if(key==DM_WASM_KEY_DOWN&&state.selected+1<state.count)dm_host_network_select((int)state.selected+1,0);
    if(key==DM_WASM_KEY_ENTER&&state.selected<state.count)dm_host_network_select((int)state.selected,1);
}
void dm_app_menu(int32_t command){if(command==DM_WASM_MENU_RELOAD)dm_host_network_action(0);}
void dm_app_close(void){}
