#include "core_installer.h"
#include "fpkg_head.h"

#ifndef DESKTOP_PREVIEW
#include <psp2/appmgr.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/promoterutil.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/sysmodule.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define INSTALL_ROOT "ux0:/data/desktop-mode/updates/core-install"
#define HELPER_ROOT "ux0:/data/desktop-mode/updates/helper-install"
#define HELPER_VPK "app0:/assets/dm-updater.vpk"
#define HELPER_TITLE "DMUPD0001"
#define HELPER_MARKER "ux0:/data/desktop-mode/updates/.updater-installed"
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

static uint16_t zip_u16(const unsigned char*p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t zip_u32(const unsigned char*p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int zip_read_at(int fd,uint32_t offset,void*data,size_t size){
    if(sceIoLseek(fd,(SceOff)offset,SCE_SEEK_SET)<0)return -1;
    unsigned char*p=data;size_t done=0;
    while(done<size){int n=sceIoRead(fd,p+done,size-done);if(n<=0)return -1;done+=(size_t)n;}
    return 0;
}
static int zip_write_all(int fd,const unsigned char*data,size_t size){
    size_t done=0;while(done<size){int n=sceIoWrite(fd,data+done,size-done);if(n<=0)return -1;done+=(size_t)n;}return 0;
}
static int extract_zip_entry(int source,int dest,uint32_t data_offset,uint32_t compressed,uint32_t expected_size,uint32_t expected_crc,uint16_t method){
    unsigned char input[16384],output[16384];uint32_t remaining=compressed,written=0;uLong crc=crc32(0L,Z_NULL,0);
    if(method==0){
        while(remaining){size_t chunk=remaining<sizeof(input)?remaining:sizeof(input);if(zip_read_at(source,data_offset,input,chunk)<0||zip_write_all(dest,input,chunk)<0)return -1;crc=crc32(crc,input,(uInt)chunk);data_offset+=(uint32_t)chunk;remaining-=(uint32_t)chunk;written+=(uint32_t)chunk;}
    }else if(method==8){
        z_stream stream;memset(&stream,0,sizeof(stream));if(inflateInit2(&stream,-MAX_WBITS)!=Z_OK)return -1;int result=Z_OK;
        while(result==Z_OK){
            if(!stream.avail_in&&remaining){uInt chunk=remaining<sizeof(input)?remaining:(uInt)sizeof(input);if(zip_read_at(source,data_offset,input,chunk)<0){result=Z_DATA_ERROR;break;}data_offset+=chunk;remaining-=chunk;stream.next_in=input;stream.avail_in=chunk;}
            stream.next_out=output;stream.avail_out=sizeof(output);result=inflate(&stream,Z_NO_FLUSH);size_t produced=sizeof(output)-stream.avail_out;
            if(produced){if(written>expected_size||produced>expected_size-written||zip_write_all(dest,output,produced)<0){result=Z_DATA_ERROR;break;}crc=crc32(crc,output,(uInt)produced);written+=(uint32_t)produced;}
            if(result==Z_BUF_ERROR&&!remaining&&!stream.avail_in)break;
        }
        inflateEnd(&stream);if(result!=Z_STREAM_END)return -1;
    }else return -1;
    return written==expected_size&&(uint32_t)crc==expected_crc?0:-1;
}

/* Parse ordinary ZIP/VPK central-directory records directly. This avoids the
 * VitaSDK minizip archive's OpenSSL-1.0-only encryption symbols; VPKs here use
 * ZIP Store/Deflate and are independently size/CRC checked before promotion. */
static int extract_vpk(const char *vpk,const char *root) {
    int zip=sceIoOpen(vpk,SCE_O_RDONLY,0);if(zip<0)return -1;
    SceOff end=sceIoLseek(zip,0,SCE_SEEK_END);if(end<22||end>UINT32_MAX){sceIoClose(zip);return -1;}
    uint32_t file_size=(uint32_t)end,tail_size=file_size<65557u?file_size:65557u,tail_offset=file_size-tail_size;
    unsigned char*tail=malloc(tail_size);if(!tail||zip_read_at(zip,tail_offset,tail,tail_size)<0){free(tail);sceIoClose(zip);return -1;}
    int eocd=-1;for(int i=(int)tail_size-22;i>=0;i--)if(zip_u32(tail+i)==0x06054b50u&&(uint32_t)i+22u+(uint32_t)zip_u16(tail+i+20)==tail_size){eocd=i;break;}
    if(eocd<0){free(tail);sceIoClose(zip);return -1;}
    uint16_t entries=zip_u16(tail+eocd+10);uint32_t cd_size=zip_u32(tail+eocd+12),cd_offset=zip_u32(tail+eocd+16);
    free(tail);if(entries>ZIP_ENTRY_LIMIT||cd_offset>file_size||cd_size>file_size-cd_offset||(uint64_t)cd_offset+cd_size>file_size){sceIoClose(zip);return -1;}
    uint64_t total=0;uint32_t cursor=cd_offset;int ok=1;char name[512],path[1024],parent[1024];unsigned char header[46],local[30];
    for(uint16_t i=0;i<entries&&ok;i++){
        if(cursor>file_size-46||zip_read_at(zip,cursor,header,sizeof(header))<0||zip_u32(header)!=0x02014b50u){ok=0;break;}
        uint16_t flags=zip_u16(header+8),method=zip_u16(header+10),name_len=zip_u16(header+28),extra_len=zip_u16(header+30),comment_len=zip_u16(header+32);
        uint32_t crc=zip_u32(header+16),compressed=zip_u32(header+20),uncompressed=zip_u32(header+24),local_offset=zip_u32(header+42);
        uint64_t next=(uint64_t)cursor+46+name_len+extra_len+comment_len;
        if(!name_len||name_len>=sizeof(name)||next>(uint64_t)cd_offset+cd_size||(flags&1)||compressed==UINT32_MAX||uncompressed==UINT32_MAX||local_offset>=file_size||uncompressed>EXTRACT_LIMIT-total){ok=0;break;}
        if(zip_read_at(zip,cursor+46,name,name_len)<0){ok=0;break;}name[name_len]=0;
        if(!safe_zip_name(name)){ok=0;break;}total+=uncompressed;
        int n=snprintf(path,sizeof(path),"%s/%s",root,name);if(n<0||(size_t)n>=sizeof(path)){ok=0;break;}
        if(name[name_len-1]=='/'){
            if(mkdirs(path)<0)ok=0;
        }else{
            if(zip_read_at(zip,local_offset,local,sizeof(local))<0||zip_u32(local)!=0x04034b50u){ok=0;break;}
            uint64_t data64=(uint64_t)local_offset+30+zip_u16(local+26)+zip_u16(local+28);
            if(data64>file_size||compressed>file_size-data64){ok=0;break;}
            memcpy(parent,path,(size_t)n+1);char*slash=strrchr(parent,'/');if(!slash){ok=0;break;}*slash=0;
            if(mkdirs(parent)<0){ok=0;break;}
            int out=sceIoOpen(path,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);if(out<0){ok=0;break;}
            ok=extract_zip_entry(zip,out,(uint32_t)data64,compressed,uncompressed,crc,method)==0;
            if(sceIoClose(out)<0)ok=0;
        }
        cursor=(uint32_t)next;
    }
    if(cursor!=(uint64_t)cd_offset+cd_size)ok=0;
    sceIoClose(zip);return ok?0:-1;
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

static int has_title_id(const char *path,const char *expected) {
    unsigned char data[8192];
    int fd=sceIoOpen(path,SCE_O_RDONLY,0);
    if(fd<0)return 0;
    int n=sceIoRead(fd,data,sizeof(data));sceIoClose(fd);
    size_t id_len=strlen(expected);
    if(n<(int)id_len)return 0;
    for(int i=0;i<=n-(int)id_len;i++)if(!memcmp(data+i,expected,id_len))return 1;
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
    if(res>=0){res=scePromoterUtilityPromotePkgWithRif(path,1);scePromoterUtilityExit();}
    sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    sceSysmoduleUnloadModuleInternalWithArg(SCE_SYSMODULE_INTERNAL_PAF,0,NULL,NULL);
    return res;
}

int dm_core_install_vpk(const char *vpk_path,const char *expected_version,int *must_exit) {
    if(must_exit)*must_exit=0;
    if(!vpk_path||!expected_version||!expected_version[0])return -1;
    remove_tree(INSTALL_ROOT);
    if(mkdirs(INSTALL_ROOT)<0)return -2;
    if(extract_vpk(vpk_path,INSTALL_ROOT)<0){remove_tree(INSTALL_ROOT);return -3;}
    char version[512],param[512],eboot[512];
    snprintf(version,sizeof(version),"%s/assets/version.txt",INSTALL_ROOT);
    snprintf(param,sizeof(param),"%s/sce_sys/param.sfo",INSTALL_ROOT);
    snprintf(eboot,sizeof(eboot),"%s/eboot.bin",INSTALL_ROOT);
    if(!file_equals(version,expected_version)||!has_title_id(param,"DSKMODE01")){remove_tree(INSTALL_ROOT);return -4;}
    SceIoStat st;
    if(sceIoGetstat(eboot,&st)<0||st.st_size<1024||sceIoGetstat(INSTALLED_ROOT,&st)<0){remove_tree(INSTALL_ROOT);return -5;}
    char marker[512];snprintf(marker,sizeof(marker),"ux0:/data/desktop-mode/updates/core-install.tag");
    int marker_fd=sceIoOpen(marker,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
    if(marker_fd<0){remove_tree(INSTALL_ROOT);return -9;}
    size_t tag_len=strlen(expected_version);
    if(sceIoWrite(marker_fd,expected_version,tag_len)!=(int)tag_len||sceIoWrite(marker_fd,"\n",1)!=1){sceIoClose(marker_fd);remove_tree(INSTALL_ROOT);return -9;}
    sceIoClose(marker_fd);
    /* Install and launch a different title. That process can safely replace us. */
    remove_tree(HELPER_ROOT);
    if(mkdirs(HELPER_ROOT)<0||extract_vpk(HELPER_VPK,HELPER_ROOT)<0){remove_tree(HELPER_ROOT);return -6;}
    char helper_sfo[512],helper_eboot[512];
    snprintf(helper_sfo,sizeof(helper_sfo),"%s/sce_sys/param.sfo",HELPER_ROOT);
    snprintf(helper_eboot,sizeof(helper_eboot),"%s/eboot.bin",HELPER_ROOT);
    if(!has_title_id(helper_sfo,HELPER_TITLE)){remove_tree(HELPER_ROOT);return -7;}
    SceIoStat helper_st;
    if(sceIoGetstat(helper_eboot,&helper_st)<0||helper_st.st_size<1024){remove_tree(HELPER_ROOT);return -8;}
    if(dm_make_head_bin(HELPER_ROOT,"app0:/assets/vitashell-head.bin",HELPER_TITLE)<0){remove_tree(HELPER_ROOT);return -10;}
    int res=promote_directory(HELPER_ROOT);
    if(res<0)return res;
    remove_tree(HELPER_ROOT);
    int helper_marker_fd=sceIoOpen(HELPER_MARKER,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
    if(helper_marker_fd<0)return -11;
    sceIoWrite(helper_marker_fd,HELPER_TITLE,sizeof(HELPER_TITLE)-1);
    sceIoClose(helper_marker_fd);
    char uri[64];
    snprintf(uri,sizeof(uri),"psgm:play?titleid=%s",HELPER_TITLE);
    res=sceAppMgrLaunchAppByUri(0xFFFFF,uri);
    if(res<0)return res;
    if(must_exit)*must_exit=1;
    return 0;
}
void dm_core_cleanup_updater(void) {
    SceIoStat st;
    if(sceIoGetstat(HELPER_MARKER,&st)<0)return;
    int res=load_paf();
    if(res<0)return;
    res=sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    if(res>=0){
        if(scePromoterUtilityInit()>=0){scePromoterUtilityDeletePkg(HELPER_TITLE);scePromoterUtilityExit();}
        sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    }
    sceSysmoduleUnloadModuleInternalWithArg(SCE_SYSMODULE_INTERNAL_PAF,0,NULL,NULL);
    sceIoRemove(HELPER_MARKER);
}
#else
int dm_core_install_vpk(const char *vpk_path,const char *expected_version,int *must_exit) {
    (void)vpk_path;(void)expected_version;if(must_exit)*must_exit=0;return -1;
}
void dm_core_cleanup_updater(void) {}
#endif
