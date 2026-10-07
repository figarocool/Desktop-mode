#include "device_ui.h"
#include "desktop_ui.h"
#include "bluetooth_native.h"
#include <stdio.h>
#ifdef DESKTOP_PREVIEW
static int preview_volume=18;
int dm_volume_get(void){return preview_volume;}
int dm_volume_set(int v){if(v<0||v>30)return -1;preview_volume=v;return 0;}
void dm_hid_counts(int*k,int*m){*k=*m=-1;}
int dm_bluetooth_settings(void){dm_status("Nell'anteprima associa i dispositivi nelle impostazioni Bluetooth di Linux");return -1;}
#else
#include <psp2/avconfig.h>
#include <psp2/hid.h>
#include <psp2/appmgr.h>
int dm_volume_get(void){int v=-1;if(sceAVConfigGetSystemVol(&v)<0)return -1;return v;}
int dm_volume_set(int v){return v>=0&&v<=30?sceAVConfigSetSystemVol(v):-1;}
void dm_hid_counts(int*k,int*m){int handles[8]={0};int r=sceHidKeyboardEnumerate(handles,8);*k=r<0?-1:0;if(r>=0)for(int i=0;i<8;i++)*k+=handles[i]>0;for(int i=0;i<8;i++)handles[i]=0;r=sceHidMouseEnumerate(handles,8);*m=r<0?-1:0;if(r>=0)for(int i=0;i<8;i++)*m+=handles[i]>0;}
int dm_bluetooth_settings(void){int r=sceAppMgrLaunchAppByUri(0xFFFFF,"settings_dlg:");if(r<0)dm_status("Impostazioni Vita non disponibili");return r;}
#endif
#ifdef DESKTOP_PREVIEW
static const char*volume_title="Volume anteprima",*hid_unavailable="Anteprima: tastiera e mouse gestiti da Linux";
#else
static const char*volume_title="Volume";
#endif
static int volume_popup,last_volume=18;
int dm_device_popup_active(void){return volume_popup;}
void dm_device_dismiss(void){volume_popup=0;}
static void set_volume(int volume){if(dm_volume_set(volume)<0)dm_status("Volume non modificabile su questo dispositivo");else if(volume)last_volume=volume;}
int dm_device_tray_click(int x,int y){
 if(y>=504&&x>=700&&x<728){volume_popup=0;dm_clock_dismiss();dm_launch("bluetooth",NULL);return 1;}
 if(y>=504&&x>=728&&x<758){volume_popup=!volume_popup;dm_clock_dismiss();return 1;}
 if(!volume_popup)return 0;
 if(x<640||x>=840||y<325||y>=504){volume_popup=0;return 0;}
 int v=dm_volume_get();if(y>=385&&y<420&&x>=655&&x<=820)set_volume((x-655)*30/165);
 if(y>=439&&y<479&&x>=660&&x<820){if(v>0){last_volume=v;set_volume(0);}else set_volume(last_volume);}
 return 1;
}
void dm_device_tray_draw(void){
 unsigned white=DM_COLOR(245,248,255,255);dm_rect(700,504,58,40,DM_COLOR(34,65,108,255));
 dm_rect(712,512,2,23,white);for(int i=0;i<8;i++){dm_rect(714+i,513+i,1,1,white);dm_rect(714+i,520-i,1,1,white);dm_rect(714+i,525+i,1,1,white);dm_rect(714+i,533-i,1,1,white);}dm_rect(706,518,2,2,white);dm_rect(706,528,2,2,white);
 dm_rect(732,520,5,9,white);for(int i=0;i<6;i++)dm_rect(737+i,519-i,1,11+i*2,white);
 int v=dm_volume_get();if(!v){dm_rect(746,518,2,13,DM_COLOR(235,95,85,255));}else{dm_rect(747,521,1,7,white);dm_rect(750,519,1,11,white);}
}
void dm_device_popup_draw(void){if(!volume_popup)return;int v=dm_volume_get();unsigned ink=DM_COLOR(24,38,55,255);dm_rect(640,325,200,179,DM_COLOR(235,243,251,255));dm_text(655,354,volume_title,ink);char s[32];snprintf(s,sizeof(s),v<0?"Non disponibile":"%d%%",v*100/30);dm_text(655,379,s,ink);dm_rect(655,398,166,4,DM_COLOR(160,175,195,255));if(v>=0)dm_rect(652+v*165/30,388,8,24,DM_COLOR(40,100,170,255));dm_rect(660,439,160,40,DM_COLOR(205,224,242,255));dm_text(675,466,dm_localize(v?"Disattiva audio":"Riattiva audio",v?"Mute":"Unmute",v?"Silenciar":"Activar sonido"),ink);}
typedef struct{int keyboards,mice,selected,prompted;unsigned timer;uint64_t forgetting_mac;} Bluetooth;
static void bt_pin_entered(const char*pin,void*ctx){DmWindow*w=ctx;if(w->used&&dm_bt_pin(pin)<0)dm_status("PIN non accettato dalla console");}
static void bt_confirmation(int yes,void*ctx){DmWindow*w=ctx;if(w->used&&dm_bt_confirm(yes)<0)dm_status("Conferma Bluetooth non accettata");}
static void bt_forget_confirmed(int yes,void*ctx){DmWindow*w=ctx;if(!w->used||!yes)return;Bluetooth*b=w->state;if(dm_bt_forget(b->forgetting_mac)<0)dm_status(dm_localize("Impossibile rimuovere il dispositivo associato","Could not forget paired device","No se pudo olvidar el dispositivo emparejado"));else dm_status(dm_localize("Dispositivo rimosso dall'elenco associati","Device removed from paired list","Dispositivo eliminado de la lista de emparejados"));dm_bt_refresh_registered();}
static void bt_tick(DmWindow*w,unsigned ms){Bluetooth*b=w->state;b->timer+=ms;dm_bt_poll();if(b->timer>=1000){b->timer=0;dm_hid_counts(&b->keyboards,&b->mice);}const DmBtState*s=dm_bt_state();if(!s->auth_kind)b->prompted=0;if(s->auth_kind&&!b->prompted&&!dm_confirm_active()){b->prompted=1;if(s->auth_kind==1)dm_prompt("PIN Bluetooth (da digitare anche sulla tastiera)","",bt_pin_entered,w);else if(dm_confirm("Associazione Bluetooth","Confermare l'associazione con il dispositivo selezionato?",bt_confirmation,w)<0)b->prompted=0;}}
static void bt_open(DmWindow*w,const char*a){(void)a;Bluetooth*b=w->state;b->selected=-1;dm_hid_counts(&b->keyboards,&b->mice);if(dm_bt_init()>=0)dm_bt_refresh_registered();}
static void bt_draw(DmWindow*w){Bluetooth*b=w->state;const DmBtState*s=dm_bt_state();unsigned ink=DM_COLOR(24,38,55,255);char text[180];int radio=dm_bt_enabled();if(b->keyboards<0||b->mice<0)snprintf(text,sizeof(text),"%s",dm_localize("Bluetooth - tastiera e mouse","Bluetooth - keyboard and mouse","Bluetooth - teclado y raton"));else snprintf(text,sizeof(text),dm_localize("Bluetooth %s | Tastiere: %d | Mouse: %d","Bluetooth %s | Keyboards: %d | Mice: %d","Bluetooth %s | Teclados: %d | Ratones: %d"),radio==1?dm_localize("attivo","on","activado"):radio==0?dm_localize("spento","off","desactivado"):"?",b->keyboards,b->mice);dm_text_raw(w->x+20,w->y+65,text,ink);
#ifdef DESKTOP_PREVIEW
 dm_text(w->x+20,w->y+95,hid_unavailable,ink);
#else
 if(s->error)snprintf(text,sizeof(text),dm_localize("API Bluetooth: errore 0x%08X","Bluetooth API: error 0x%08X","API Bluetooth: error 0x%08X"),(unsigned)s->error);else snprintf(text,sizeof(text),"%s",dm_localize(s->scanning?"Ricerca dispositivi in corso...":"Seleziona un dispositivo per associarlo",s->scanning?"Searching for devices...":"Select a device to pair",s->scanning?"Buscando dispositivos...":"Selecciona un dispositivo para emparejar"));dm_text(w->x+20,w->y+95,text,ink);
#endif
 int visible=(w->h-225)/28;if(visible>DM_BT_MAX)visible=DM_BT_MAX;for(int i=0;i<s->count&&i<visible;i++){if(b->selected==i)dm_rect(w->x+20,w->y+110+i*28,w->w-40,28,DM_COLOR(180,213,245,255));const char*state=s->devices[i].connection==5?dm_localize("Connesso","Connected","Conectado"):s->devices[i].connection==2?dm_localize("Connessione...","Connecting...","Conectando..."):dm_localize("Disponibile","Available","Disponible");snprintf(text,sizeof(text),"%.56s | %s%s",s->devices[i].name,state,s->devices[i].registered?dm_localize(" | Associato"," | Paired"," | Emparejado"):"");dm_text_raw(w->x+25,w->y+130+i*28,text,ink);}
 const char*labels[]={"Bluetooth","Cerca","Associa","Disconnetti","Dimentica","Aggiorna","Impostazioni"};int left=14,gap=4,bw=(w->w-28-6*gap)/7;for(int i=0;i<7;i++){int x=left+i*(bw+gap);dm_rect(w->x+x,w->y+w->h-90,bw,35,DM_COLOR(205,224,242,255));const char*label=i==0?(radio==1?dm_localize("Spegni BT","BT off","Apagar BT"):dm_localize("Accendi BT","BT on","Encender BT")):dm_ui_translate(labels[i]);dm_text_center(w->x+x+bw/2,w->y+w->h-66,label,ink);}dm_text(w->x+20,w->y+w->h-30,dm_localize("Tastiera italiana | Compatibilita dipendente dal dispositivo","Italian keyboard | Compatibility depends on device","Teclado italiano | Compatibilidad segun el dispositivo"),ink);}
static void bt_click(DmWindow*w,int x,int y){Bluetooth*b=w->state;const DmBtState*s=dm_bt_state();if(y>=110&&y<w->h-110&&x>=20&&x<w->w-20){int n=(y-110)/28;if(n<s->count)b->selected=n;return;}if(y>=w->h-90&&y<w->h-55&&x>=14&&x<w->w-14){int gap=4,bw=(w->w-28-6*gap)/7,action=(x-14)/(bw+gap),r=0;if(action==0){int enabled=dm_bt_enabled();if(enabled>=0)r=dm_bt_set_enabled(!enabled);}if(action==1)r=dm_bt_scan();if(action==2)r=dm_bt_connect(b->selected);if(action==3)r=dm_bt_disconnect(b->selected);if(action==4){if(b->selected<0||b->selected>=s->count||!s->devices[b->selected].registered){dm_status(dm_localize("Seleziona un dispositivo associato","Select a paired device","Selecciona un dispositivo emparejado"));return;}b->forgetting_mac=s->devices[b->selected].mac;if(dm_confirm("Dimentica dispositivo Bluetooth","Rimuovere questo dispositivo dall'elenco associati?",bt_forget_confirmed,w)<0)dm_status("Conferma non disponibile");return;}if(action==5){r=dm_bt_refresh_registered();if(r>=0)dm_status(dm_localize("Elenco dispositivi associati aggiornato","Paired devices refreshed","Lista de dispositivos emparejados actualizada"));}if(action==6)r=dm_bluetooth_settings();if(r<0)dm_status("Operazione Bluetooth non riuscita; verifica che il Bluetooth sia attivo");}}
static int bt_close(DmWindow*w){(void)w;dm_bt_shutdown();return 1;}
const DmApp dm_bluetooth_app={DM_API_VERSION,"bluetooth","Bluetooth e dispositivi",sizeof(Bluetooth),bt_open,bt_draw,bt_click,0,0,bt_close,"",bt_tick};

void dm_device_tick(int x,int y,int held){if(volume_popup&&held&&x>=655&&x<=820&&y>=385&&y<420)set_volume((x-655)*30/165);}
