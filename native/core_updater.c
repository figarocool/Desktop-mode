#include <psp2/appmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/promoterutil.h>
#include <psp2/sysmodule.h>
#include "fpkg_head.h"
#include <stdio.h>
#include <string.h>

#define TITLE_ID "DMUPD0001"
#define INSTALL_ROOT "ux0:/data/desktop-mode/updates/core-install"
#define VERSION_FILE INSTALL_ROOT "/assets/version.txt"
#define FAILURE_FILE "ux0:/data/desktop-mode/updates/.core-install-failed"
#define VERSION_MARKER "ux0:/data/desktop-mode/updates/core-install.tag"

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
    int result=promote_core();
    if(result<0)report_failure(result);
    else sceIoRemove(FAILURE_FILE);
    sceAppMgrLaunchAppByUri(0xFFFFF,"psgm:play?titleid=DSKMODE01");
    return 0;
}
