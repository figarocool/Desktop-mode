#include "desktop_ui.h"
#include <stdio.h>
#include <string.h>
int dm_ascii_casecmp(const char*a,const char*b){
    const unsigned char*x=(const unsigned char*)(a?a:""),*y=(const unsigned char*)(b?b:"");
    for(;;x++,y++){
        unsigned char c=*x,d=*y;
        if(c>='A'&&c<='Z')c=(unsigned char)(c+('a'-'A'));
        if(d>='A'&&d<='Z')d=(unsigned char)(d+('a'-'A'));
        if(c!=d)return c<d?-1:1;
        if(!c)return 0;
    }
}
#ifdef DESKTOP_PREVIEW
#include <SDL.h>
uint64_t dm_clock_ms(void) {
    return SDL_GetTicks64();
}
time_t dm_system_local_epoch(void) {
    return time(NULL);
}
void dm_system_info(DmSystemInfo*i) {
    memset(i,0,sizeof(*i));
    i->preview=1;
    i->battery_percent=-1;
    i->charging=-1;
    int seconds,percent;
    SDL_PowerState power=SDL_GetPowerInfo(&seconds,&percent);
    if(percent>=0) {
        i->battery_percent=percent;
        i->charging=power==SDL_POWERSTATE_CHARGING;
    }
    snprintf(i->model,sizeof(i->model),"Anteprima Linux (dati Vita non disponibili)");
    snprintf(i->firmware,sizeof(i->firmware),"Backend SDL2");
    snprintf(i->ip,sizeof(i->ip),"Non disponibile nell'anteprima");
}
int dm_system_network_init(void){return 0;}
#else
#include <psp2/power.h>
#include <psp2/rtc.h>
#include <psp2/appmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <stdlib.h>
static int network_stack_state;
static void *network_stack_memory;
int dm_system_network_init(void) {
    if(network_stack_state>0)return 0;
    if(network_stack_state<0)return -1;
    network_stack_memory=malloc(256*1024);
    if(!network_stack_memory){network_stack_state=-1;return -1;}
    SceNetInitParam param={network_stack_memory,256*1024,0};
    int result=sceNetInit(&param);
    if(result<0){free(network_stack_memory);network_stack_memory=NULL;network_stack_state=-1;return result;}
    sceNetCtlInit();
    network_stack_state=1;
    return 0;
}
uint64_t dm_clock_ms(void) {
    return sceKernelGetProcessTimeWide()/1000;
}
time_t dm_system_local_epoch(void) {
    SceDateTime date;
    if(sceRtcGetCurrentClockLocalTime(&date)<0)return time(NULL);
    struct tm t= {
        0
    };
    t.tm_year=date.year-1900;
    t.tm_mon=date.month-1;
    t.tm_mday=date.day;
    t.tm_hour=date.hour;
    t.tm_min=date.minute;
    t.tm_sec=date.second;
    t.tm_isdst=-1;
    return mktime(&t);
}
void dm_system_info(DmSystemInfo*i) {
    memset(i,0,sizeof(*i));
    dm_system_network_init();
    i->battery_percent=scePowerGetBatteryLifePercent();
    i->charging=scePowerIsBatteryCharging();
    i->cpu_mhz=scePowerGetArmClockFrequency();
    i->gpu_mhz=scePowerGetGpuClockFrequency();
    snprintf(i->model,sizeof(i->model),"%s",sceKernelGetModel()==SCE_KERNEL_MODEL_VITATV?"PlayStation TV":"PS Vita");
    SceKernelSystemSwVersion v;
    memset(&v,0,sizeof(v));
    v.size=sizeof(v);
    if(sceKernelGetSystemSwVersion(&v)>=0)snprintf(i->firmware,sizeof(i->firmware),"%s",v.versionString);
    else strcpy(i->firmware,"Non disponibile");
    SceKernelFreeMemorySizeInfo mem= {
        sizeof(mem),0,0,0
    };
    if(sceKernelGetFreeMemorySize(&mem)>=0)i->ram_free=mem.size_user;
    uint64_t total=0,free_space=0;
    if(sceAppMgrGetDevInfo("ux0:",&total,&free_space)>=0) {
        i->storage_total=total;
        i->storage_free=free_space;
    }
    SceNetCtlInfo net;
    memset(&net,0,sizeof(net));
    if(sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS,&net)>=0)snprintf(i->ip,sizeof(i->ip),"%s",net.ip_address);
    else strcpy(i->ip,"Wi-Fi non connesso");
}
#endif
