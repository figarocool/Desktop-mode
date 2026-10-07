#include "network_settings.h"
#include "desktop_ui.h"
#include "device_ui.h"
#include "preferences.h"
#include <vita2d.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef DESKTOP_PREVIEW
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netpacket/packet.h>
#include <unistd.h>
void dm_network_adapter(DmNetworkAdapter*a){
    memset(a,0,sizeof(*a));a->preview=1;a->state=a->wifi=a->signal=a->mtu=a->wifi_radio=-1;
    struct ifaddrs*all=NULL;if(getifaddrs(&all)<0){a->error=-1;return;}
    for(struct ifaddrs*i=all;i;i=i->ifa_next){
        if(!i->ifa_addr||i->ifa_addr->sa_family!=AF_INET||(i->ifa_flags&IFF_LOOPBACK)||!(i->ifa_flags&IFF_UP))continue;
        snprintf(a->name,sizeof(a->name),"%s",i->ifa_name);a->state=3;
        inet_ntop(AF_INET,&((struct sockaddr_in*)i->ifa_addr)->sin_addr,a->ip,sizeof(a->ip));
        if(i->ifa_netmask)inet_ntop(AF_INET,&((struct sockaddr_in*)i->ifa_netmask)->sin_addr,a->mask,sizeof(a->mask));
        char path[256];snprintf(path,sizeof(path),"/sys/class/net/%s/wireless",i->ifa_name);a->wifi=access(path,F_OK)==0;
        snprintf(path,sizeof(path),"/sys/class/net/%s/mtu",i->ifa_name);FILE*f=fopen(path,"r");if(f){if(fscanf(f,"%d",&a->mtu)!=1)a->mtu=-1;fclose(f);}break;
    }
    for(struct ifaddrs*i=all;i;i=i->ifa_next)if(i->ifa_addr&&i->ifa_addr->sa_family==AF_PACKET&&!strcmp(i->ifa_name,a->name)){struct sockaddr_ll*s=(struct sockaddr_ll*)i->ifa_addr;if(s->sll_halen==6)snprintf(a->mac,sizeof(a->mac),"%02X:%02X:%02X:%02X:%02X:%02X",s->sll_addr[0],s->sll_addr[1],s->sll_addr[2],s->sll_addr[3],s->sll_addr[4],s->sll_addr[5]);}
    freeifaddrs(all);if(!a->name[0])a->state=0;
    FILE*f=fopen("/proc/net/route","r");if(f){char line[512],name[64];unsigned long destination,gateway;unsigned flags;while(fgets(line,sizeof(line),f))if(sscanf(line,"%63s %lx %lx %x",name,&destination,&gateway,&flags)==4&&!destination&&(flags&2)&&!strcmp(name,a->name)){unsigned char address[4]={gateway&255,(gateway>>8)&255,(gateway>>16)&255,(gateway>>24)&255};inet_ntop(AF_INET,address,a->gateway,sizeof(a->gateway));break;}fclose(f);}
    f=fopen("/etc/resolv.conf","r");if(f){char line[512],address[64];int n=0;while(n<2&&fgets(line,sizeof(line),f))if(sscanf(line,"nameserver %63s",address)==1){struct in_addr ip;if(inet_pton(AF_INET,address,&ip)==1)inet_ntop(AF_INET,&ip,a->dns[n++],16);}fclose(f);}
}
int dm_network_configure(int mode){(void)mode;dm_status("Anteprima: cambia rete dalle impostazioni di Linux");return -1;}
int dm_wifi_radio_enabled(void){return -1;}
int dm_wifi_radio_set(int enabled){(void)enabled;dm_status("Anteprima: radio Wi-Fi non controllabile da Linux");return -1;}
int dm_wifi_scan_start(void){dm_status("La scansione Wi-Fi e disponibile solo sulla Vita");return -1;}
int dm_wifi_scan_poll(DmWifiAccessPoint*out,int capacity){(void)out;(void)capacity;return -1;}
int dm_wifi_connect(const char*ssid,const char*password){(void)ssid;(void)password;return -1;}
#else
#include <psp2/net/netctl.h>
#include <psp2/appmgr.h>
extern int sceWlanGetConfiguration(void);
extern int sceWlanSetConfiguration(int);
extern int dm_netctl_job_register(void(*callback)(int,void*),void*,int*);
extern int dm_netctl_scan_start(const void*,int,void*);
extern int dm_netctl_scan_wait(int,int*);
extern int dm_netctl_scan_count(void);
extern int dm_netctl_scan_get(int,void*);
extern int dm_netctl_profile_submit(void*);
static volatile int wifi_scan_done;
static int wifi_scan_callback=-1,wifi_scan_pending;
static void wifi_scan_event(int event,void*arg){(void)arg;if(event==4)wifi_scan_done=1;}
int dm_wifi_scan_start(void){
    if(wifi_scan_pending)return -2;
    if(dm_wifi_radio_enabled()!=1)return -3;
    if(wifi_scan_callback<0){int id=-1;int r=dm_netctl_job_register(wifi_scan_event,NULL,&id);if(r<0)return r;wifi_scan_callback=id;}
    wifi_scan_done=0;
    int r=dm_netctl_scan_start(NULL,0,NULL);
    if(r<0)return r;
    wifi_scan_pending=1;
    return 0;
}
int dm_wifi_scan_poll(DmWifiAccessPoint*out,int capacity){
    if(!wifi_scan_pending||!out||capacity<1)return -1;
    sceNetCtlCheckCallback();
    if(!wifi_scan_done)return 0;
    wifi_scan_pending=0;
    int wait_result=0;int r=dm_netctl_scan_wait(4,&wait_result);if(r<0)return r;if(wait_result<0)return wait_result;
    int count=dm_netctl_scan_count();if(count<0)return count;if(count>DM_WIFI_MAX_ACCESS_POINTS)count=DM_WIFI_MAX_ACCESS_POINTS;if(count>capacity)count=capacity;
    for(int i=0;i<count;i++){
        unsigned char record[0x54];memset(record,0,sizeof(record));
        r=dm_netctl_scan_get(i,record);if(r<0)return r;
        memcpy(out[i].ssid,record+0x18,32);out[i].ssid[32]=0;
        for(int n=31;n>=0&&(out[i].ssid[n]==0||out[i].ssid[n]==' ');n--)out[i].ssid[n]=0;
        memcpy(&out[i].security,record+0x10,sizeof(out[i].security));
        memcpy(&out[i].signal,record+0x3c,sizeof(out[i].signal));
    }
    return count+1; /* zero remains the asynchronous "still scanning" result */
}
int dm_wifi_connect(const char*ssid,const char*password){
    if(!ssid||!ssid[0]||!password)return -1;
    size_t ssid_len=strnlen(ssid,33),password_len=strnlen(password,65);
    if(ssid_len>32||password_len>64)return -1;
    unsigned char request[0x608];memset(request,0,sizeof(request));
    /* SceSettings' profile-submit request: SSID at 0x50, request flag at
       0xBC, and the optional key as the second user pointer/length pair. */
    memcpy(request+0x50,ssid,ssid_len);
    request[0xBC]=1;
    static const char default_ip[16]="192.168.123.123";
    static const char default_mask[16]="255.255.255.0";
    memcpy(request+0x2C0,default_ip,sizeof(default_ip));
    memcpy(request+0x2D0,default_mask,sizeof(default_mask));
    if(password_len){uint32_t key=(uint32_t)(uintptr_t)password,length=(uint32_t)password_len;memcpy(request+0x5D0,&key,sizeof(key));memcpy(request+0x5D4,&length,sizeof(length));}
    int r=dm_netctl_profile_submit(request);
    memset(request,0,sizeof(request));
    return r;
}
int dm_wifi_radio_enabled(void){int r=sceWlanGetConfiguration();return r<0?r:(r&1)!=0;}
int dm_wifi_radio_set(int enabled){if(enabled!=0&&enabled!=1)return -1;int r=sceWlanSetConfiguration(enabled?3:2);if(r<0)dm_status("Impossibile modificare la radio Wi-Fi");return r;}
static int number(int code,int*value){SceNetCtlInfo i;memset(&i,0,sizeof(i));int r=sceNetCtlInetGetInfo(code,&i);if(r>=0)*value=(int)i.device;return r;}
static void address(int code,char*out,size_t capacity){SceNetCtlInfo i;memset(&i,0,sizeof(i));if(sceNetCtlInetGetInfo(code,&i)>=0){size_t length=strnlen(i.cnf_name,capacity-1);memcpy(out,i.cnf_name,length);out[length]=0;}}
void dm_network_adapter(DmNetworkAdapter*a){
    DmSystemInfo system;dm_system_info(&system); /* shared native network initialization */
    memset(a,0,sizeof(*a));a->state=a->wifi=a->signal=a->mtu=-1;
    a->wifi_radio=dm_wifi_radio_enabled();
    int r=sceNetCtlInetGetState(&a->state);if(r<0)a->error=r;
    address(SCE_NETCTL_INFO_GET_CNF_NAME,a->name,sizeof(a->name));address(SCE_NETCTL_INFO_GET_SSID,a->ssid,sizeof(a->ssid));
    int device=-1;if(number(SCE_NETCTL_INFO_GET_DEVICE,&device)>=0){if(device==0)a->wifi=1;else if(device==1)a->wifi=0;}
    if(a->ssid[0])a->wifi=1;
    number(SCE_NETCTL_INFO_GET_RSSI_PERCENTAGE,&a->signal);number(SCE_NETCTL_INFO_GET_MTU,&a->mtu);
    if(a->signal<0||a->signal>100)a->signal=-1;
    address(SCE_NETCTL_INFO_GET_IP_ADDRESS,a->ip,sizeof(a->ip));address(SCE_NETCTL_INFO_GET_NETMASK,a->mask,sizeof(a->mask));address(SCE_NETCTL_INFO_GET_DEFAULT_ROUTE,a->gateway,sizeof(a->gateway));
    address(SCE_NETCTL_INFO_GET_PRIMARY_DNS,a->dns[0],16);address(SCE_NETCTL_INFO_GET_SECONDARY_DNS,a->dns[1],16);
    SceNetCtlInfo mac;memset(&mac,0,sizeof(mac));if(sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_ETHER_ADDR,&mac)>=0){unsigned char*p=mac.ether_addr.data;snprintf(a->mac,sizeof(a->mac),"%02X:%02X:%02X:%02X:%02X:%02X",p[0],p[1],p[2],p[3],p[4],p[5]);}
    int proxy=-1;if(number(SCE_NETCTL_INFO_GET_HTTP_PROXY_CONFIG,&proxy)>=0&&proxy>0){SceNetCtlInfo info;memset(&info,0,sizeof(info));if(sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_HTTP_PROXY_SERVER,&info)>=0){int port=-1;number(SCE_NETCTL_INFO_GET_HTTP_PROXY_PORT,&port);snprintf(a->proxy,sizeof(a->proxy),"%.100s:%d",info.http_proxy_server,port);}}
}
int dm_network_configure(int mode){
    (void)mode;
    dm_status(dm_localize("Configurazione profili Wi-Fi non ancora disponibile","Wi-Fi profile configuration is not available yet","La configuracion de perfiles Wi-Fi aun no esta disponible"));
    return -1;
}
#endif
static DmNetworkAdapter tray;static uint64_t checked;static int initialized;
void dm_network_tray_draw(void){
    uint64_t now=dm_clock_ms();if(!initialized||now-checked>=2000){dm_network_adapter(&tray);checked=now;initialized=1;}
    unsigned color=tray.state==3?DM_COLOR(145,225,175,255):tray.state<0?DM_COLOR(185,195,210,255):DM_COLOR(245,245,250,255);
    dm_rect(670,504,30,40,DM_COLOR(34,65,108,255));
    if(tray.wifi==0){dm_rect(675,515,19,13,color);dm_rect(677,517,15,9,DM_COLOR(34,65,108,255));dm_rect(683,528,3,4,color);dm_rect(679,532,11,2,color);}
    else{for(int i=0;i<4;i++){int height=5+i*4;unsigned c=tray.state==3&&(tray.signal<0||tray.signal>=i*25)?color:DM_COLOR(112,126,147,255);dm_rect(674+i*5,533-height,3,height,c);}}
    if(tray.state==0){dm_line(674,514,694,534,DM_COLOR(245,105,95,255));dm_line(694,514,674,534,DM_COLOR(245,105,95,255));}
}
int dm_network_tray_click(int x,int y){if(x<670||x>=700||y<504||y>=544)return 0;dm_clock_dismiss();dm_device_dismiss();dm_launch("connections",NULL);return 1;}
typedef struct {DmNetworkAdapter adapter;DmWifiAccessPoint aps[DM_WIFI_MAX_ACCESS_POINTS];unsigned elapsed;int scan_pending,scan_count,scan_offset,scan_selected,scan_error;} Connection;
static void open_connection(DmWindow*w,const char*arg){(void)arg;dm_network_adapter(&((Connection*)w->state)->adapter);}
static void connect_password(const char*password,void*context){Connection*s=context;if(!s||s->scan_selected<0||s->scan_selected>=s->scan_count)return;int r=dm_wifi_connect(s->aps[s->scan_selected].ssid,password);if(r<0){char message[96];snprintf(message,sizeof(message),"Profilo Wi-Fi non accettato (0x%08X)",(unsigned)r);dm_status(message);return;}dm_status(dm_localize("Richiesta di connessione inviata; verifica lo stato rete.","Connection request sent; check network status.","Solicitud enviada; comprueba el estado de red."));dm_network_adapter(&s->adapter);}
static void tick(DmWindow*w,unsigned ms){Connection*s=w->state;s->elapsed+=ms;if(s->elapsed>=2000){s->elapsed=0;dm_network_adapter(&s->adapter);}if(s->scan_pending){int r=dm_wifi_scan_poll(s->aps,DM_WIFI_MAX_ACCESS_POINTS);if(r>0){s->scan_count=r-1;s->scan_selected=0;s->scan_pending=0;s->scan_error=0;}else if(r<0){s->scan_pending=0;s->scan_error=r;dm_status("Scansione Wi-Fi fallita: controlla radio e firmware");}}}
static const char*value(const char*s){return s[0]?s:dm_localize("Non disponibile","Unavailable","No disponible");}
static void draw(DmWindow*w){
    Connection*s=w->state;DmNetworkAdapter*a=&s->adapter;unsigned ink=DM_COLOR(24,38,55,255);char text[256];
    const char*status=a->state==3?dm_localize("Connessa","Connected","Conectada"):a->state==0?dm_localize("Disconnessa","Disconnected","Desconectada"):a->state==1||a->state==2?dm_localize("Connessione in corso","Connecting","Conectando"):dm_localize("Non disponibile","Unavailable","No disponible");
    snprintf(text,sizeof(text),"%s: %s | %s",dm_localize("Scheda di rete","Network adapter","Adaptador de red"),a->wifi==1?"Wi-Fi":a->wifi==0?"Ethernet / LAN":"Rete",status);dm_text(w->x+20,w->y+62,text,ink);
    snprintf(text,sizeof(text),"%s: %.64s",dm_localize("Connessione","Connection","Conexion"),value(a->name));dm_text(w->x+20,w->y+94,text,ink);
    snprintf(text,sizeof(text),"SSID: %.32s  |  MAC: %s",a->wifi==1?value(a->ssid):"--",value(a->mac));dm_text(w->x+20,w->y+126,text,ink);
    const char*labels[]={"IP",dm_localize("Subnet mask","Subnet mask","Mascara de subred"),dm_localize("Gateway","Gateway","Puerta de enlace"),"DNS 1","DNS 2"};const char*values[]={a->ip,a->mask,a->gateway,a->dns[0],a->dns[1]};for(int i=0;i<5;i++){snprintf(text,sizeof(text),"%s: %s",labels[i],value(values[i]));dm_text_raw(w->x+20+(i%2)*310,w->y+163+(i/2)*30,text,ink);}
    snprintf(text,sizeof(text),"MTU: %s",a->mtu>=0?"":"--");if(a->mtu>=0)snprintf(text,sizeof(text),"MTU: %d",a->mtu);dm_text(w->x+330,w->y+223,text,ink);
    if(a->wifi==1&&a->signal>=0)snprintf(text,sizeof(text),"%s: %d%%",dm_localize("Segnale Wi-Fi","Wi-Fi signal","Senal Wi-Fi"),a->signal);else snprintf(text,sizeof(text),"Proxy: %.100s",a->preview?"gestito da Linux":value(a->proxy));dm_text(w->x+20,w->y+255,text,ink);
    snprintf(text,sizeof(text),"Wi-Fi: %s",a->wifi_radio==1?dm_localize("attivo","on","activado"):a->wifi_radio==0?dm_localize("spento","off","desactivado"):dm_localize("stato non disponibile","state unavailable","estado no disponible"));dm_text(w->x+20,w->y+288,text,ink);
    if(a->preview)dm_text(w->x+20,w->y+315,dm_localize("Anteprima Linux: scansione access point disponibile solo su Vita.","Linux preview: access point scan is available on Vita only.","Vista previa Linux: buscar puntos de acceso solo esta disponible en Vita."),ink);
    else {snprintf(text,sizeof(text),"%s%s",s->scan_pending?dm_localize("Ricerca reti Wi-Fi in corso...","Scanning for Wi-Fi networks...","Buscando redes Wi-Fi..."):dm_localize("Reti Wi-Fi rilevate:","Available Wi-Fi networks:","Redes Wi-Fi detectadas:"),s->scan_error?" (errore)":"");dm_text(w->x+20,w->y+315,text,ink);}
    if(s->scan_count>2)dm_text(w->x+w->w-78,w->y+315,"<  >",ink);
    for(int i=0;i<2&&i+s->scan_offset<s->scan_count;i++){int index=i+s->scan_offset,row_y=w->y+335+i*22;dm_rect(w->x+18,row_y-14,w->w-36,20,s->scan_selected==index?DM_COLOR(171,207,243,255):DM_COLOR(215,226,239,255));snprintf(text,sizeof(text),"%.32s   sicurezza %08X   %d",s->aps[index].ssid,(unsigned)s->aps[index].security,s->aps[index].signal);dm_text(w->x+24,row_y,text,ink);}
    const char*buttons[]={a->wifi_radio==1?dm_localize("Spegni Wi-Fi","Turn Wi-Fi off","Apagar Wi-Fi"):dm_localize("Accendi Wi-Fi","Turn Wi-Fi on","Encender Wi-Fi"),dm_localize("Cerca reti","Scan networks","Buscar redes"),dm_localize("Connetti","Connect","Conectar"),dm_localize("DHCP / IP","DHCP / IP","DHCP / IP"),dm_ui_translate("Aggiorna")};
    int left=14,gap=5,available=w->w-2*left-4*gap,bw=available/5;
    for(int i=0;i<5;i++){int x=left+i*(bw+gap);dm_rect(w->x+x,w->y+w->h-50,bw,32,DM_COLOR(195,216,238,255));dm_text_center(w->x+x+bw/2,w->y+w->h-27,buttons[i],ink);}
}
static void click(DmWindow*w,int x,int y){Connection*s=w->state;if(y>=308&&y<328&&x>=w->w-86&&x<w->w-18&&s->scan_count>2){if(x<w->w-50)s->scan_offset=s->scan_offset>=2?s->scan_offset-2:0;else if(s->scan_offset+2<s->scan_count)s->scan_offset+=2;return;}if(y>=321&&y<365&&x>=18&&x<w->w-18){int row=(y-321)/22,index=row+s->scan_offset;if(row>=0&&row<2&&index<s->scan_count)s->scan_selected=index;return;}if(y<w->h-50||y>=w->h-18||x<14||x>=w->w-14)return;int gap=5,bw=(w->w-28-4*gap)/5,action=(x-14)/(bw+gap);if(action>4)return;if(action==0){int enabled=dm_wifi_radio_enabled();if(enabled>=0){if(dm_wifi_radio_set(!enabled)>=0)dm_network_adapter(&s->adapter);}}if(action==1){s->scan_count=0;s->scan_error=0;s->scan_offset=0;int r=dm_wifi_scan_start();if(r>=0)s->scan_pending=1;else{s->scan_error=r;dm_status("Avvio scansione Wi-Fi non riuscito");}}if(action==2){if(s->scan_selected<0||s->scan_selected>=s->scan_count){dm_status("Seleziona prima una rete Wi-Fi");return;}dm_prompt(dm_localize("Password della rete Wi-Fi","Wi-Fi network password","Contrasena de la red Wi-Fi"),"",connect_password,s);}if(action==3)dm_network_configure(2);if(action==4)dm_network_adapter(&s->adapter);}
const DmApp dm_network_settings_app={DM_API_VERSION,"connections","Connessioni di rete",sizeof(Connection),open_connection,draw,click,NULL,NULL,NULL,NULL,tick};
