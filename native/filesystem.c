#include "desktop_api.h"
#include "rename.h"
#include "file_jobs.h"
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
extern void dm_files_changed(void);
int dm_fs_writable(const char*p) {
    const char*roots[]= {
        "ux0:/","uma0:/","imc0:/","ur0:/data/","ud0:/"
    };
    for(unsigned i=0;i<sizeof(roots)/sizeof(*roots);i++)if(!strncmp(p,roots[i],strlen(roots[i])))return strstr(p,"/../")==NULL&&strstr(p,"/./")==NULL&&!(strlen(p)>=3&&!strcmp(p+strlen(p)-3,"/.."));
    return 0;
}
int dm_fs_join(char*out,size_t cap,const char*dir,const char*name) {
    if(!dir||!name||!name[0]||strchr(name,'/')||strchr(name,'\\')||strchr(name,':')||!strcmp(name,".")||!strcmp(name,".."))return -1;
    for(const unsigned char*p=(const unsigned char*)name;*p;p++)if(*p<32)return -1;
    int n=snprintf(out,cap,"%s%s%s",dir,dir[0]&&dir[strlen(dir)-1]!='/'?"/":"",name);
    return n<0||(size_t)n>=cap?-1:0;
}
void dm_fs_parent(char*p) {
    size_t n=strlen(p);
    if(n&&p[n-1]=='/')p[--n]=0;
    char*s=strrchr(p,'/');
    if(s)s[1]=0;
    else p[0]=0;
}
int dm_fs_is_directory(const char*p) {
    int fd=sceIoDopen(p);
    if(fd<0)return 0;
    sceIoDclose(fd);
    return 1;
}
int dm_fs_read(const char*p,char*b,size_t cap) {
    if(!cap)return -1;
    int fd=sceIoOpen(p,SCE_O_RDONLY,0);
    if(fd<0)return fd;
    size_t total=0;
    int n=0;
    while(total<cap-1&&(n=sceIoRead(fd,b+total,cap-1-total))>0)total+=n;
    char extra;
    int more=n<0?n:sceIoRead(fd,&extra,1);
    sceIoClose(fd);
    b[total]=0;
    if(n<0||more<0)return -1;
    return more>0?-2:(int)total;
}
int dm_fs_write(const char*p,const void*data,size_t length,int exclusive) {
    if(!dm_fs_writable(p))return -1;
    int fd=sceIoOpen(p,SCE_O_WRONLY|SCE_O_CREAT|(exclusive?SCE_O_EXCL:SCE_O_TRUNC),0666);
    if(fd<0)return fd;
    size_t total=0;
    while(total<length) {
        int n=sceIoWrite(fd,(const char*)data+total,length-total);
        if(n<=0) {
            sceIoClose(fd);
            if(exclusive)sceIoRemove(p);
            return -1;
        }
        total+=n;
    }
    int result=sceIoClose(fd);
    if(result>=0)dm_files_changed();
    else if(exclusive)sceIoRemove(p);
    return result;
}
static int copy_tree(const char*src,const char*dst,int depth) {
    if(depth>32||!strcmp(src,dst)||!dm_fs_writable(dst))return -1;
    if(dm_fs_is_directory(src)) {
        if(sceIoMkdir(dst,0777)<0)return -1;
        int fd=sceIoDopen(src);
        if(fd<0)return -1;
        SceIoDirent e;
        int result=0,n;
        memset(&e,0,sizeof(e));
        while((n=sceIoDread(fd,&e))>0) {
            if(strcmp(e.d_name,".")&&strcmp(e.d_name,"..")) {
                char a[DM_PATH_MAX],b[DM_PATH_MAX];
                if(dm_fs_join(a,sizeof(a),src,e.d_name)||dm_fs_join(b,sizeof(b),dst,e.d_name)||copy_tree(a,b,depth+1)) {
                    result=-1;
                    break;
                }
            }
            memset(&e,0,sizeof(e));
        }
        if(n<0)result=-1;
        sceIoDclose(fd);
        return result;
    }
    int in=sceIoOpen(src,SCE_O_RDONLY,0);
    if(in<0)return -1;
    int out=sceIoOpen(dst,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_EXCL,0666);
    if(out<0) {
        sceIoClose(in);
        return -1;
    }
    char buffer[16384];
    int n,result=0;
    while((n=sceIoRead(in,buffer,sizeof(buffer)))>0) {
        int pos=0;
        while(pos<n) {
            int w=sceIoWrite(out,buffer+pos,n-pos);
            if(w<=0) {
                result=-1;
                break;
            }
            pos+=w;
        }
        if(result)break;
    }
    if(n<0)result=-1;
    sceIoClose(in);
    if(sceIoClose(out)<0)result=-1;
    if(result)sceIoRemove(dst);
    return result;
}
int dm_fs_copy(const char*src,const char*dst) {
    size_t n=strlen(src);
    while(n&&src[n-1]=='/')n--;
    if(!strncasecmp(src,dst,n)&&(dst[n]=='/'||dst[n]==0))return -1;
    return copy_tree(src,dst,0);
}

extern int dm_files_renamed(const char*,const char*);
static struct {char source[DM_PATH_MAX],destination[DM_PATH_MAX];uint64_t revision;} rename_events[32];
static uint64_t rename_revision;
void dm_fs_path_update(char*path,size_t capacity,uint64_t*seen){
 if(!path||!seen)return;
 uint64_t first=rename_revision>32?rename_revision-31:1;
 if(*seen+1>first)first=*seen+1;
 for(uint64_t revision=first;revision<=rename_revision;revision++){
  unsigned index=(revision-1)%32;
  if(rename_events[index].revision==revision)dm_path_renamed(path,capacity,rename_events[index].source,rename_events[index].destination);
 }
 *seen=rename_revision;
}
static int relocate(const char*source,const char*destination,int moving){
 if(dm_job_active())return -1;
 if(!source||!destination||strlen(source)>=DM_PATH_MAX||strlen(destination)>=DM_PATH_MAX||!dm_fs_writable(source)||!dm_fs_writable(destination)||strstr(source,"/desktop-mode/Trash/")||strstr(destination,"/desktop-mode/Trash/"))return -1;
 char parent[DM_PATH_MAX],target_parent[DM_PATH_MAX];snprintf(parent,sizeof(parent),"%s",source);snprintf(target_parent,sizeof(target_parent),"%s",destination);
 dm_fs_parent(parent);dm_fs_parent(target_parent);if(!moving&&strcmp(parent,target_parent))return -1;
 const char*source_colon=strchr(source,':'),*destination_colon=strchr(destination,':');if(!source_colon||!destination_colon||source_colon-source!=destination_colon-destination||strncmp(source,destination,source_colon-source))return -1;
 size_t source_length=strlen(source);if(!strncmp(source,destination,source_length)&&(destination[source_length]=='/'||!destination[source_length]))return -1;
 const char*colon=strchr(source,':');const char*name=strrchr(source,'/');const char*new_name=strrchr(destination,'/');
 if(!colon||!name||!name[1]||!new_name||!new_name[1])return -1;
 char checked[DM_PATH_MAX];if(dm_fs_join(checked,sizeof(checked),target_parent,new_name+1)<0||strcmp(checked,destination))return -1;
 char store[DM_PATH_MAX];snprintf(store,sizeof(store),"%.*s/data/desktop-mode",(int)(colon-source+1),source);
 size_t length=strlen(source);if(!strncmp(store,source,length)&&(store[length]=='/'||!store[length]))return -1;
 if(!strcmp(source,"ux0:/data/desktop-mode/Desktop"))return -1;
 SceIoStat stat;if(sceIoGetstat(source,&stat)<0)return -1;if(!strcmp(source,destination))return 0;
 if(sceIoGetstat(destination,&stat)>=0)return -1;
 if(sceIoRename(source,destination)<0)return -1;
 if(dm_files_renamed(source,destination)<0){sceIoRename(destination,source);return -1;}
 unsigned index=rename_revision++%32;
 snprintf(rename_events[index].source,DM_PATH_MAX,"%s",source);snprintf(rename_events[index].destination,DM_PATH_MAX,"%s",destination);rename_events[index].revision=rename_revision;
 dm_files_changed();return 0;
}

int dm_fs_rename(const char*source,const char*destination){if(source&&destination&&!strcmp(source,destination))return 0;return relocate(source,destination,0);}
int dm_fs_move(const char*source,const char*destination){return relocate(source,destination,1);}
int dm_fs_list(const char*path,DmDirectoryEntry*out,int capacity,int offset){
 if(!path||!out||capacity<1||capacity>2048||offset<0)return -1;
 int fd=sceIoDopen(path);if(fd<0)return -1;int count=0,index=0,status;SceIoDirent e;
 while(count<capacity){memset(&e,0,sizeof(e));status=sceIoDread(fd,&e);if(status<=0){if(status<0)count=-1;break;}if(!strcmp(e.d_name,".")||!strcmp(e.d_name,".."))continue;if(index++<offset)continue;snprintf(out[count].name,sizeof(out[count].name),"%s",e.d_name);out[count].size=e.d_stat.st_size;out[count].directory=SCE_S_ISDIR(e.d_stat.st_mode);count++;}
 sceIoDclose(fd);return count;
}
int dm_fs_mkdir(const char*path){
 if(!path||!dm_fs_writable(path))return -1;
 char parent[DM_PATH_MAX],checked[DM_PATH_MAX];if(strlen(path)>=sizeof(parent))return -1;snprintf(parent,sizeof(parent),"%s",path);dm_fs_parent(parent);const char*name=strrchr(path,'/');if(!name||dm_fs_join(checked,sizeof(checked),parent,name+1)||strcmp(checked,path))return -1;int result=sceIoMkdir(path,0777);if(result>=0)dm_files_changed();return result;
}

int dm_copy_async(const char*s,const char*d,DmOperationResult cb,void*ctx){return dm_job_copy(s,d,cb,ctx);}
int dm_trash_async(const char*s,DmOperationResult cb,void*ctx){return dm_job_trash(s,cb,ctx);}
