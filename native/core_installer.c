#include "core_installer.h"

#ifndef DESKTOP_PREVIEW
#include <minizip/unzip.h>
#include <psp2/appmgr.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/promoterutil.h>
#include <psp2/sysmodule.h>
#include <stdio.h>
#include <string.h>

#define INSTALL_ROOT "ux0:/data/desktop-mode/updates/core-install"
#define INSTALLED_ROOT "ux0:/app/DSKMODE01"
#define EXTRACT_LIMIT (48u * 1024u * 1024u)
#define ZIP_ENTRY_LIMIT 4096u

static int mkdirs(const char *path) {
    char buffer[512];
    size_t n=strlen(path);
    if(!n||n>=sizeof(buffer))return -1;
    memcpy(buffer,path,n+1);
    for(size_t i=0;i<n;i++)if(buffer[i]=='/'){
        if(i==0)continue;
        char saved=buffer[i];buffer[i]=0;
        sceIoMkdir(buffer,0777);
        buffer[i]=saved;
    }
    return sceIoMkdir(buffer,0777)<0&&sceIoGetstat(buffer,&(SceIoStat){0})<0?-1:0;
}

static void remove_tree(const char *path) {
    SceIoStat st;
    if(sceIoGetstat(path,&st)<0)return;
    if(SCE_S_ISDIR(st.st_mode)){
        int dir=sceIoDopen(path);
        if(dir>=0){
            SceIoDirent ent;
            while(sceIoDread(dir,&ent)>0){
                if(!strcmp(ent.d_name,".")||!strcmp(ent.d_name,".."))continue;
                char child[512];
                int n=snprintf(child,sizeof(child),"%s/%s",path,ent.d_name);
                if(n>0&&(size_t)n<sizeof(child))remove_tree(child);
            }
            sceIoDclose(dir);
        }
        sceIoRmdir(path);
    }else sceIoRemove(path);
}

static int safe_zip_name(const char *name) {
    if(!name||!*name||name[0]=='/'||strchr(name,'\\')||strstr(name,":")||strstr(name,"../"))return 0;
    const char *p=name;
    while(*p){
        const char *slash=strchr(p,'/');size_t len=slash?(size_t)(slash-p):strlen(p);
        if(len==2&&p[0]=='.'&&p[1]=='.')return 0;
        if(!len)return slash&&slash[1]==0;
        p=slash?slash+1:p+len;
    }
    return 1;
}

static int extract_vpk(const char *vpk) {
    unzFile zip=unzOpen64(vpk);
    if(!zip)return -1;
    unz_global_info64 global;
    if(unzGetGlobalInfo64(zip,&global)!=UNZ_OK||global.number_entry>ZIP_ENTRY_LIMIT||unzGoToFirstFile(zip)!=UNZ_OK){unzClose(zip);return -1;}
    uint64_t total=0;
    int ok=1;
    char name[512],path[1024],buffer[16384];
    for(uint64_t i=0;i<global.number_entry&&ok;i++){
        unz_file_info64 info;
        if(unzGetCurrentFileInfo64(zip,&info,name,sizeof(name),NULL,0,NULL,0)!=UNZ_OK||info.size_filename>=sizeof(name)||!safe_zip_name(name)||info.uncompressed_size>EXTRACT_LIMIT-total){ok=0;break;}
        total+=info.uncompressed_size;
        int n=snprintf(path,sizeof(path),"%s/%s",INSTALL_ROOT,name);
        if(n<0||(size_t)n>=sizeof(path)){ok=0;break;}
        size_t name_len=strlen(name);
        if(name_len&&name[name_len-1]=='/'){
            if(mkdirs(path)<0)ok=0;
        }else{
            char parent[1024];memcpy(parent,path,(size_t)n+1);
            char *slash=strrchr(parent,'/');if(!slash){ok=0;break;}*slash=0;
            if(mkdirs(parent)<0||unzOpenCurrentFile(zip)!=UNZ_OK){ok=0;break;}
            int fd=sceIoOpen(path,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
            if(fd<0){unzCloseCurrentFile(zip);ok=0;break;}
            uint64_t written=0;int read_n;
            while((read_n=unzReadCurrentFile(zip,buffer,sizeof(buffer)))>0){
                int at=0;while(at<read_n){int wrote=sceIoWrite(fd,buffer+at,(size_t)(read_n-at));if(wrote<=0){ok=0;break;}at+=wrote;}
                if(!ok)break;
                written+=(uint64_t)read_n;
            }
            if(read_n<0||written!=info.uncompressed_size||unzCloseCurrentFile(zip)!=UNZ_OK)ok=0;
            if(sceIoClose(fd)<0)ok=0;
        }
        if(i+1<global.number_entry&&unzGoToNextFile(zip)!=UNZ_OK)ok=0;
    }
    if(unzClose(zip)!=UNZ_OK)ok=0;
    return ok?0:-1;
}

static int file_equals(const char *path,const char *expected) {
    char data[96]={0};
    int fd=sceIoOpen(path,SCE_O_RDONLY,0);
    if(fd<0)return 0;
    int n=sceIoRead(fd,data,sizeof(data)-1);sceIoClose(fd);
    if(n<0)return 0;
    data[n]=0;data[strcspn(data,"\r\n")]=0;
    return !strcmp(data,expected);
}

static int has_title_id(const char *path) {
    unsigned char data[8192];
    int fd=sceIoOpen(path,SCE_O_RDONLY,0);
    if(fd<0)return 0;
    int n=sceIoRead(fd,data,sizeof(data));sceIoClose(fd);
    static const char id[]="DSKMODE01";
    if(n<(int)sizeof(id)-1)return 0;
    for(int i=0;i<=n-(int)sizeof(id)+1;i++)if(!memcmp(data+i,id,sizeof(id)-1))return 1;
    return 0;
}

static int load_paf(void) {
    /* VitaShell uses this internal PAF load argument layout for package promotion. */
    uint32_t args[6]={0x180000u,UINT32_MAX,UINT32_MAX,1u,UINT32_MAX,UINT32_MAX};
    int result=-1;
    uint32_t option[4]={(uint32_t)sizeof(option),(uint32_t)&result,UINT32_MAX,UINT32_MAX};
    return sceSysmoduleLoadModuleInternalWithArg(SCE_SYSMODULE_INTERNAL_PAF,sizeof(args),args,(SceSysmoduleOpt *)option);
}

static int promote_directory(const char *path) {
    int res=load_paf();
    if(res<0)return res;
    res=sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    if(res<0){sceSysmoduleUnloadModuleInternalWithArg(SCE_SYSMODULE_INTERNAL_PAF,0,NULL,NULL);return res;}
    res=scePromoterUtilityInit();
    if(res>=0){res=scePromoterUtilityPromotePkg(path,1);scePromoterUtilityExit();}
    sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    sceSysmoduleUnloadModuleInternalWithArg(SCE_SYSMODULE_INTERNAL_PAF,0,NULL,NULL);
    return res;
}

int dm_core_install_vpk(const char *vpk_path,const char *expected_version,int *must_exit) {
    if(must_exit)*must_exit=0;
    if(!vpk_path||!expected_version||!expected_version[0])return -1;
    remove_tree(INSTALL_ROOT);
    if(mkdirs(INSTALL_ROOT)<0)return -2;
    if(extract_vpk(vpk_path)<0){remove_tree(INSTALL_ROOT);return -3;}
    char version[512],param[512],eboot[512];
    snprintf(version,sizeof(version),"%s/assets/version.txt",INSTALL_ROOT);
    snprintf(param,sizeof(param),"%s/sce_sys/param.sfo",INSTALL_ROOT);
    snprintf(eboot,sizeof(eboot),"%s/eboot.bin",INSTALL_ROOT);
    if(!file_equals(version,expected_version)||!has_title_id(param)){remove_tree(INSTALL_ROOT);return -4;}
    SceIoStat st;
    if(sceIoGetstat(eboot,&st)<0||st.st_size<1024||sceIoGetstat(INSTALLED_ROOT,&st)<0){remove_tree(INSTALL_ROOT);return -5;}
    /* Release app0 before asking the native promoter to replace this title. */
    if(sceAppMgrUmount("app0:")<0){remove_tree(INSTALL_ROOT);return -6;}
    if(must_exit)*must_exit=1;
    int res=promote_directory(INSTALL_ROOT);
    if(res<0)return res;
    return 0;
}
#else
int dm_core_install_vpk(const char *vpk_path,const char *expected_version,int *must_exit) {
    (void)vpk_path;(void)expected_version;if(must_exit)*must_exit=0;return -1;
}
#endif
