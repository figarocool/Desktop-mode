#include <psp2/appmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/promoterutil.h>
#include <psp2/sysmodule.h>
#include <vita2d.h>
#include "fpkg_head.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define TITLE_ID "DMUPD0001"
#define INSTALL_ROOT "ux0:/data/desktop-mode/updates/core-install"
#define VERSION_FILE INSTALL_ROOT "/assets/version.txt"
#define FAILURE_FILE "ux0:/data/desktop-mode/updates/.core-install-failed"
#define VERSION_MARKER "ux0:/data/desktop-mode/updates/core-install.tag"
#define DM_COLOR(r,g,b,a) ((uint32_t)(r)|((uint32_t)(g)<<8)|((uint32_t)(b)<<16)|((uint32_t)(a)<<24))

static vita2d_pgf *font;
static volatile int install_done;
static int install_result;
static int promote_core(void);

static void draw_install_screen(const char *message,uint64_t tick) {
    vita2d_start_drawing();vita2d_clear_screen();
    vita2d_draw_rectangle(0,0,960,544,DM_COLOR(15,35,66,255));
    vita2d_draw_rectangle(0,0,960,7,DM_COLOR(68,145,218,255));
    vita2d_draw_rectangle(136,72,688,402,DM_COLOR(25,51,86,255));
    vita2d_draw_rectangle(136,72,688,2,DM_COLOR(104,166,221,255));
    if(font){
        vita2d_pgf_draw_text(font,480-vita2d_pgf_text_width(font,0.9f,"Aggiornamento Desktop Mode")/2,132,DM_COLOR(245,250,255,255),0.9f,"Aggiornamento Desktop Mode");
        vita2d_pgf_draw_text(font,175,178,DM_COLOR(218,232,248,255),0.72f,message?message:"Preparazione installazione...");
        vita2d_pgf_draw_text(font,175,221,DM_COLOR(255,255,255,255),0.62f,"File sostituiti dal pacchetto:");
        vita2d_pgf_draw_text(font,195,253,DM_COLOR(211,228,246,255),0.58f,"ux0:/app/DSKMODE01/eboot.bin  -  programma core");
        vita2d_pgf_draw_text(font,195,280,DM_COLOR(211,228,246,255),0.58f,"ux0:/app/DSKMODE01/apps/*.dmapp  -  app incluse");
        vita2d_pgf_draw_text(font,195,307,DM_COLOR(211,228,246,255),0.58f,"ux0:/app/DSKMODE01/assets e sce_sys  -  risorse");
        vita2d_pgf_draw_text(font,175,339,DM_COLOR(157,204,243,255),0.58f,"I dati utente in ux0:/data/desktop-mode/ restano invariati.");
    }
    vita2d_draw_rectangle(175,365,610,22,DM_COLOR(9,23,43,255));
    vita2d_draw_rectangle(177,367,606,18,DM_COLOR(41,68,101,255));
    int offset=(int)((tick/8u)%700u)-100;if(offset<0)offset=0;if(offset>510)offset=510;
    vita2d_draw_rectangle(177+offset,367,96,18,DM_COLOR(67,159,239,255));
    if(font)vita2d_pgf_draw_text(font,480-vita2d_pgf_text_width(font,0.58f,"Attendere, installazione in corso...")/2,419,DM_COLOR(220,237,255,255),0.58f,"Attendere, installazione in corso...");
    vita2d_end_drawing();vita2d_swap_buffers();
}

static int install_worker(SceSize args,void *argp) {
    (void)args;(void)argp;
    install_result=promote_core();
    install_done=1;
    return 0;
}

static int load_paf(void) {
    uint32_t args[6]={0x180000u,UINT32_MAX,UINT32_MAX,1u,UINT32_MAX,UINT32_MAX};
    int result=-1;
    uint32_t option[4]={(uint32_t)sizeof(option),(uint32_t)&result,UINT32_MAX,UINT32_MAX};
    return sceSysmoduleLoadModuleInternalWithArg(SCE_SYSMODULE_INTERNAL_PAF,sizeof(args),args,(SceSysmoduleOpt *)option);
}

static int contains_title(const char *path,const char *id) {
    unsigned char data[8192];
    int fd=sceIoOpen(path,SCE_O_RDONLY,0);if(fd<0)return 0;
    int n=sceIoRead(fd,data,sizeof(data));sceIoClose(fd);
    size_t len=strlen(id);
    for(int i=0;i< n-(int)len+1;i++)if(!memcmp(data+i,id,len))return 1;
    return 0;
}

static int same_version(void) {
    char expected[64]={0},actual[64]={0};
    int fd=sceIoOpen(VERSION_MARKER,SCE_O_RDONLY,0);if(fd<0)return 0;
    int n=sceIoRead(fd,expected,sizeof(expected)-1);sceIoClose(fd);if(n<=0)return 0;
    expected[n]=0;expected[strcspn(expected,"\r\n")]=0;
    fd=sceIoOpen(VERSION_FILE,SCE_O_RDONLY,0);if(fd<0)return 0;
    n=sceIoRead(fd,actual,sizeof(actual)-1);sceIoClose(fd);if(n<=0)return 0;
    actual[n]=0;actual[strcspn(actual,"\r\n")]=0;
    return expected[0]&&!strcmp(expected,actual);
}

static void report_failure(int error) {
    char tag[64]={0},line[96];
    int fd=sceIoOpen(VERSION_MARKER,SCE_O_RDONLY,0);
    if(fd>=0){int n=sceIoRead(fd,tag,sizeof(tag)-1);sceIoClose(fd);if(n>0)tag[n]=0;}
    tag[strcspn(tag,"\r\n")]=0;
    fd=sceIoOpen(FAILURE_FILE,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
    if(fd>=0){int n=snprintf(line,sizeof(line),"%s %08X\n",tag,(unsigned)error);if(n>0&&(size_t)n<sizeof(line))sceIoWrite(fd,line,(size_t)n);sceIoClose(fd);}
}

static int promote_core(void) {
    char param[128],eboot[128];SceIoStat st;
    snprintf(param,sizeof(param),"%s/sce_sys/param.sfo",INSTALL_ROOT);
    snprintf(eboot,sizeof(eboot),"%s/eboot.bin",INSTALL_ROOT);
    if(!same_version()||!contains_title(param,"DSKMODE01")||sceIoGetstat(eboot,&st)<0||st.st_size<1024)return -1;
    if(dm_make_head_bin(INSTALL_ROOT,"app0:/assets/vitashell-head.bin","DSKMODE01")<0)return -2;
    int res=load_paf();if(res<0)return res;
    res=sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    if(res<0){sceSysmoduleUnloadModuleInternalWithArg(SCE_SYSMODULE_INTERNAL_PAF,0,NULL,NULL);return res;}
    res=scePromoterUtilityInit();
    if(res>=0){res=scePromoterUtilityPromotePkgWithRif(INSTALL_ROOT,1);scePromoterUtilityExit();}
    sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    sceSysmoduleUnloadModuleInternalWithArg(SCE_SYSMODULE_INTERNAL_PAF,0,NULL,NULL);
    return res;
}

int main(void) {
    sceAppMgrDestroyOtherApp();
    vita2d_init();font=vita2d_load_default_pgf();
    draw_install_screen("Verifica pacchetto e versione...",0);
    SceUID worker=sceKernelCreateThread("dm-core-install",install_worker,0x10000100,64*1024,0,0,NULL);
    int result;
    if(worker>=0&&sceKernelStartThread(worker,0,NULL)>=0){
        uint64_t tick=0;
        while(!install_done){draw_install_screen("Installazione del pacchetto DSKMODE01...",tick++*16u);sceKernelDelayThread(16000);}
        sceKernelWaitThreadEnd(worker,NULL,NULL);sceKernelDeleteThread(worker);result=install_result;
    }else{
        draw_install_screen("Installazione del pacchetto DSKMODE01...",0);
        result=promote_core();
    }
    if(result<0)report_failure(result);
    else sceIoRemove(FAILURE_FILE);
    draw_install_screen(result<0?"Installazione non riuscita; verra riaperta la versione precedente.":"Installazione completata; riavvio Desktop Mode...",0);
    sceKernelDelayThread(900000);
    sceAppMgrLaunchAppByUri(0xFFFFF,"psgm:play?titleid=DSKMODE01");
    return 0;
}
