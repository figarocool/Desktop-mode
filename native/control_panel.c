#include "desktop_api.h"
#include "device_ui.h"
#include "preferences.h"
#include "rdp_server.h"
#include <stdio.h>
#include <string.h>
typedef struct {
    DmSystemInfo info;
    unsigned timer;
}
Panel;
static void open_panel(DmWindow*w,const char*arg) {
    (void)arg;
    dm_system_info(&((Panel*)w->state)->info);
}
static void tick_panel(DmWindow*w,unsigned elapsed) {
    Panel*p=w->state;
    p->timer+=elapsed;
    if(p->timer>=2000) {
        p->timer=0;
        dm_system_info(&p->info);
    }
}
static void draw_fit(DmWindow*w,int x,int baseline,const char*text,int width,uint32_t color) {
    char clipped[128];snprintf(clipped,sizeof(clipped),"%.127s",text);
    while(clipped[0]&&dm_text_width(clipped)>width){size_t n=strlen(clipped);do{n--;}while(n&&((unsigned char)clipped[n]&0xc0)==0x80);clipped[n]=0;}
    dm_text_raw(w->x+x, w->y+baseline, clipped,color);
}
static void draw_panel(DmWindow*w) {
    Panel*p=w->state;
    DmSystemInfo*i=&p->info;
    char lines[10][120];
    snprintf(lines[0],120,dm_localize("Console: %s","Console: %s","Consola: %s"),i->model);
    snprintf(lines[1],120,dm_localize("Firmware: %s","Firmware: %s","Firmware: %s"),i->firmware);
    if(i->battery_percent>=0)snprintf(lines[2],120,dm_localize("Batteria: %d%% | CPU %d MHz | GPU %d MHz","Battery: %d%% | CPU %d MHz | GPU %d MHz","Bateria: %d%% | CPU %d MHz | GPU %d MHz"),i->battery_percent,i->cpu_mhz,i->gpu_mhz);
    else snprintf(lines[2],120,"%s",dm_localize("Batteria / clock: disponibili sulla console","Battery / clocks: available on console","Bateria / frecuencias: disponibles en la consola"));
    snprintf(lines[3],120,dm_localize("RAM libera: %llu MiB","Free RAM: %llu MiB","RAM libre: %llu MiB"),(unsigned long long)(i->ram_free/1048576));
    snprintf(lines[4],120,dm_localize("ux0: %llu MiB liberi / %llu MiB totali","ux0: %llu MiB free / %llu MiB total","ux0: %llu MiB libres / %llu MiB totales"),(unsigned long long)(i->storage_free/1048576),(unsigned long long)(i->storage_total/1048576));
    if(i->preview) {
        snprintf(lines[3],120,"%s",dm_localize("RAM Vita: non disponibile nell'anteprima","Vita RAM: unavailable in preview","RAM de Vita: no disponible en la vista previa"));
        snprintf(lines[4],120,"%s",dm_localize("Spazio ux0: disponibile sulla console","ux0 storage: available on console","Almacenamiento ux0: disponible en la consola"));
    }
    snprintf(lines[5],120,dm_localize("IP: %s","IP: %s","IP: %s"),i->ip);
    snprintf(lines[6],120,"App esterne caricate: %d | API v%d",dm_loaded_plugin_count(),DM_API_VERSION);
    snprintf(lines[7],120,"Desktop remoto RDP: %s",dm_rdp_enabled()?"abilitato":"disabilitato");
    for(int n=0;n<6;n++)draw_fit(w,20,65+n*27,lines[n],w->w-40,DM_COLOR(24,38,55,255));
    static const char*labels[4][2]={
        {"Applicazioni installate","Bluetooth"},
        {"Impostazioni desktop","Tipi di file"},
        {"Cambia sfondo","Cerca nuove app"},
        {"Desktop remoto RDP","Connessioni di rete"}
    };
    int col_width=(w->w-56)/2;
    if(col_width<1)return;
    for(int row=0;row<4;row++){
        int y=252+row*36;
        if(y+30>w->h-8)continue;
        for(int col=0;col<2;col++){
            int x=20+col*(col_width+16);
            const char*label=dm_ui_translate(labels[row][col]);
            dm_rect(w->x+x,w->y+y,col_width,30,DM_COLOR(198,219,240,255));
            draw_fit(w,x+10,y+21,label,col_width-20,DM_COLOR(24,38,55,255));
        }
    }
}
static void click_panel(DmWindow*w,int x,int y) {
    int col_width=(w->w-56)/2;
    if(col_width<1||x<20||x>=w->w-20||y<252)return;
    int row=(y-252)/36;
    if(row>=4||y>=252+row*36+30||252+row*36+30>w->h-8)return;
    int col=x<20+col_width?0:x>=36+col_width?1:-1;
    if(col<0)return;
    if(row==0)dm_launch(col?"bluetooth":"apps",NULL);
    else if(row==1)dm_launch(col?"filetypes":"settings",NULL);
    else if(row==2){if(col)dm_scan_plugins();else dm_launch("personalize",NULL);}
    else dm_launch(col?"connections":"remote",NULL);
}
const DmApp dm_control_panel_app= {
    DM_API_VERSION,"control","Pannello di controllo",sizeof(Panel),open_panel,draw_panel,click_panel,0,0,0,0,tick_panel
};
