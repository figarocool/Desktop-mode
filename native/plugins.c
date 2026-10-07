#include "desktop_api.h"
#include "preferences.h"
#include "app_manager.h"
#include "rename.h"
#include "file_jobs.h"
#include "wasm_app_runtime.h"
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef DESKTOP_PREVIEW
#include <dlfcn.h>
#define PLATFORM_ID 2
#define MODULE_SUFFIX "so"
#else
#include <psp2/kernel/modulemgr.h>
#include <psp2/sysmodule.h>
#define PLATFORM_ID 1
#define MODULE_SUFFIX "suprx"
#endif
static DmInstalledApp loaded[DM_APP_LIMIT];
#define REMOVED "ux0:/data/desktop-mode/removed-apps.txt"
static char removed_path[DM_PATH_MAX]=REMOVED;
static char removed[DM_APP_LIMIT][DM_PATH_MAX];
static int removed_count,removed_initialized;
static void read_removed(void) {
    if(removed_initialized)return;
    removed_initialized=1;
    char data[24577];
    if(dm_fs_read(removed_path,data,sizeof(data))<0)return;
    char*save,*line=strtok_r(data,"\n",&save);
    while(line&&removed_count<DM_APP_LIMIT) {
        snprintf(removed[removed_count++],DM_PATH_MAX,"%s",line);
        line=strtok_r(NULL,"\n",&save);
    }
}
static int is_removed(const char*path) {
    read_removed();
    for(int i=0;i<removed_count;i++)if(!strcmp(removed[i],path))return 1;
    return 0;
}
static int write_removed(void) {
    char data[24577];
    data[0]=0;
    for(int i=0;i<removed_count;i++) {
        strcat(data,removed[i]);
        strcat(data,"\n");
    }
    return dm_fs_write(removed_path,data,strlen(data),0);
}
static int loaded_count;
static unsigned extraction_sequence;
static char plugin_diagnostic[192];
const char *dm_plugin_diagnostic(void){return plugin_diagnostic;}
int dm_plugin_bundle_removed(const char*id){char path[DM_PATH_MAX];if(!id||!id[0])return 0;int n=snprintf(path,sizeof(path),"app0:/apps/%s.dmapp",id);return n>0&&(size_t)n<sizeof(path)?is_removed(path):0;}
static const char*plugin_log_path="ux0:/data/desktop-mode/app-loader.log";
static void plugin_log_reset(void){
    sceIoMkdir("ux0:/data/desktop-mode",0777);
    int fd=sceIoOpen(plugin_log_path,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
    if(fd>=0){const char*header="Desktop Mode app loader log\n";sceIoWrite(fd,header,strlen(header));sceIoClose(fd);}
}
static void plugin_log(const char*kind,const char*path,const char*detail){
    SceIoStat st;
    if(sceIoGetstat(plugin_log_path,&st)>=0&&st.st_size>32768)plugin_log_reset();
    int fd=sceIoOpen(plugin_log_path,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_APPEND,0666);
    if(fd<0)return;
    char line[1400];
    int n=snprintf(line,sizeof(line),"%llu %s %s%s%s\n",(unsigned long long)dm_clock_ms(),kind,path?path:"-",detail?" | ":"",detail?detail:"");
    if(n>0){size_t size=(size_t)n<sizeof(line)?(size_t)n:sizeof(line)-1;sceIoWrite(fd,line,size);}
    sceIoClose(fd);
}
const DmHostAPI dm_host_api= {
    DM_API_VERSION,sizeof(DmHostAPI),dm_register_app,dm_launch,dm_focus,dm_close,dm_maximize,dm_associate_extension,dm_open_file,dm_rect,dm_text,dm_status,dm_prompt,dm_clipboard_text_set,dm_clipboard_text_get,dm_fs_read,dm_fs_write,dm_fs_copy,dm_fs_is_directory,dm_fs_join,dm_fs_writable,dm_fs_parent,dm_system_info,dm_clock_ms,dm_file_dialog,dm_confirm,dm_text_width,dm_text_center,dm_pointer_state,dm_fs_rename,dm_fs_path_update,dm_tasks,dm_image_load,dm_image_create,dm_image_update,dm_image_draw,dm_image_size,dm_image_free,dm_image_save_png,dm_fs_list,dm_fs_mkdir,dm_copy_async,dm_trash_async,dm_language,dm_localize,dm_tray_set,dm_memory_register,dm_app_memory,dm_register_screensaver,dm_text_raw,dm_text_width_raw,dm_ui_translate,dm_scrollbar_draw
};
int dm_loaded_plugin_count(void) {
    int active=0;
    for(int i=0;i<loaded_count;i++)active+=loaded[i].installed;
    return active;
}
static unsigned le32(const unsigned char*p) {
    return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;
}
static int load_wasm_package(const char *path,const unsigned char header[32]) {
    unsigned size=le32(header+16);
    if(memcmp(header,"DMAPP001",8)||le32(header+8)!=0||le32(header+12)!=DM_API_VERSION||le32(header+20)!=0||le32(header+24)!=1||le32(header+28)!=1||!size||size>2*1024*1024) {
        snprintf(plugin_diagnostic,sizeof(plugin_diagnostic),"%.100s: pacchetto WASM non valido",path);
        plugin_log("FAIL",path,"marcatore WASM, ABI, piattaforma o dimensione non validi");
        return -1;
    }
    uint8_t *bytes=malloc(size);
    if(!bytes)return -1;
    int fd=sceIoOpen(path,SCE_O_RDONLY,0);
    int result=-1;
    if(fd>=0&&sceIoLseek(fd,32,SCE_SEEK_SET)>=0) {
        unsigned total=0;
        while(total<size){int n=sceIoRead(fd,bytes+total,size-total);if(n<=0)break;total+=(unsigned)n;}
        if(total==size) {
            const char *base=strrchr(path,'/');base=base?base+1:path;
            char id[64],title[96];size_t n=strlen(base);const char *ext=strrchr(base,'.');if(ext)n=(size_t)(ext-base);if(!n||n>=sizeof(id))n=0;
            if(n){memcpy(id,base,n);id[n]=0;snprintf(title,sizeof(title),"%s",!strcmp(id,"notepad")?"Notepad":!strcmp(id,"paint")?"Paint":!strcmp(id,"console")?"Console":!strcmp(id,"browser")?"Browser":!strcmp(id,"network")?"Rete locale / SMB":!strcmp(id,"solitaire")?"Solitario":!strcmp(id,"minesweeper")?"Campo minato":!strcmp(id,"pdf")?"Visualizzatore PDF":!strcmp(id,"counter")?"Contatore":!strcmp(id,"calculator")?"Calcolatrice":!strcmp(id,"taskmanager")?"Task Manager":!strcmp(id,"images")?"Anteprima immagini":!strcmp(id,"media")?"Lettore multimediale":id);result=dm_wasm_register_package(id,title,bytes,size);}
        }
    }
    if(fd>=0)sceIoClose(fd);
    free(bytes);
    if(result<0){snprintf(plugin_diagnostic,sizeof(plugin_diagnostic),"%.100s: caricamento WASM fallito",path);plugin_log("FAIL",path,"il modulo non rispetta ABI o import Desktop");return -1;}
    return 0;
}
static int extract(const char*path,char*cache) {
    int in=sceIoOpen(path,SCE_O_RDONLY,0);
    if(in<0)return -1;
    unsigned char header[32];
    if(sceIoRead(in,header,32)!=32||memcmp(header,strstr(path,".dmsaver")?"DMSAV001":"DMAPP001",8)||le32(header+8)!=PLATFORM_ID||le32(header+12)!=DM_API_VERSION||le32(header+20)||le32(header+24)!=0||le32(header+28)!=0||!le32(header+16)||le32(header+16)>32*1024*1024) {
        sceIoClose(in);
        dm_status("Pacchetto .dmapp non valido, API o piattaforma incompatibile");
        return -1;
    }
    unsigned remaining=le32(header+16);
    sceIoMkdir("ux0:/data/desktop-mode/cache",0777);
    int out=-1;
    for(int i=0;i<1000&&out<0;i++) {
        snprintf(cache,DM_PATH_MAX,"ux0:/data/desktop-mode/cache/module-%llu-%u.%s",(unsigned long long)dm_clock_ms(),++extraction_sequence,MODULE_SUFFIX);
        out=sceIoOpen(cache,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_EXCL,0666);
    }
    if(out<0) {
        sceIoClose(in);
        return -1;
    }
    char data[16384];
    int result=0;
    while(remaining) {
        unsigned requested=remaining>sizeof(data)?sizeof(data):remaining;
        int n=sceIoRead(in,data,requested);
        if(n<=0) {
            result=-1;
            break;
        }
        int pos=0;
        while(pos<n) {
            int written=sceIoWrite(out,data+pos,n-pos);
            if(written<=0) {
                result=-1;
                break;
            }
            pos+=written;
        }
        if(result)break;
        remaining-=n;
    }
    char extra;
    if(!result&&sceIoRead(in,&extra,1)!=0)result=-1;
    sceIoClose(in);
    if(sceIoClose(out)<0)result=-1;
    if(result)sceIoRemove(cache);
    return result;
}
static int app_id_registered(const char*id){for(int i=0;i<dm_registered_count();i++){const DmApp*a=dm_registered_app(i);if(a&&a->id&&!strcmp(a->id,id))return 1;}return 0;}
int dm_load_plugin(const char*path) {
    const char*extension=strrchr(path,'.');
    if(!extension||strcasecmp(extension,".dmapp")) {
        dm_status("Le app Desktop Mode usano l'estensione .dmapp");
        plugin_log("REJECT",path,"estensione non supportata");
        return -1;
    }
    if(is_removed(path)) {
        dm_status("App disinstallata: reinstalla dal pannello Applicazioni");
        plugin_log("SKIP",path,"segnata come disinstallata");
        return -1;
    }
    for(int i=0;i<loaded_count;i++)if(!strcmp(loaded[i].path,path)) {
        if(loaded[i].installed)return 0;
        if(loaded[i].app&&dm_register_app(loaded[i].app)==0){loaded[i].installed=1;return 0;}
        return -1;
    }
    if(!strncmp(path,"app0:/",6)){
        const char*base=strrchr(path,'/');base=base?base+1:path;char id[64];size_t n=strlen(base);const char*dot=strrchr(base,'.');if(dot)n=(size_t)(dot-base);
        if(n&&n<sizeof(id)){memcpy(id,base,n);id[n]=0;if(app_id_registered(id)){plugin_log("SKIP",path,"sostituita da un pacchetto ux0:/data/desktop-mode/apps");return 0;}}
    }
    unsigned char package_header[32];
    int package_fd=sceIoOpen(path,SCE_O_RDONLY,0);
    int header_ok=package_fd>=0&&sceIoRead(package_fd,package_header,sizeof(package_header))==(int)sizeof(package_header);
    if(package_fd>=0)sceIoClose(package_fd);
    if(header_ok&&le32(package_header+24)==1) {
        if(loaded_count>=DM_APP_LIMIT||load_wasm_package(path,package_header)<0)return -1;
        const char *base=strrchr(path,'/');base=base?base+1:path;
        char id[64];size_t n=strlen(base);const char *dot=strrchr(base,'.');if(dot)n=(size_t)(dot-base);if(!n||n>=sizeof(id))return -1;memcpy(id,base,n);id[n]=0;
        DmInstalledApp *record=&loaded[loaded_count++];memset(record,0,sizeof(*record));snprintf(record->path,sizeof(record->path),"%s",path);record->app=dm_registered_app(dm_registered_count()-1);record->installed=1;record->bundled=!strncmp(path,"app0:",5);
        plugin_diagnostic[0]=0;plugin_log("OK",path,id);return 0;
    }
    for(int i=0;i<loaded_count;i++)if(!strcmp(loaded[i].path,path)) {
        if(loaded[i].installed)return 0;
        if(dm_register_app(loaded[i].app)<0)return -1;
        loaded[i].installed=1;
        return 0;
    }
    if(loaded_count>=DM_APP_LIMIT) {
        dm_status("Limite di 48 app raggiunto");
        plugin_log("FAIL",path,"limite di 48 app raggiunto");
        return -1;
    }
    char cache[DM_PATH_MAX];
    if(extract(path,cache)<0) {
        snprintf(plugin_diagnostic,sizeof(plugin_diagnostic),"%.120s: pacchetto .dmapp illeggibile/incompatibile",path);
        dm_status("App non caricata: pacchetto incompatibile o incompleto");
        plugin_log("FAIL",path,"pacchetto .dmapp illeggibile/incompatibile");
        return -1;
    }
    int before=dm_registered_count();
#ifdef DESKTOP_PREVIEW
    char file[DM_PATH_MAX+128];
    snprintf(file,sizeof(file),"native/desktop/demo/ux0/%s",cache+5);
    void*handle=dlopen(file,RTLD_NOW|RTLD_LOCAL);
    if(!handle) {
        fprintf(stderr,"app %s: %s\n",path,dlerror());
        sceIoRemove(cache);
        snprintf(plugin_diagnostic,sizeof(plugin_diagnostic),"%.100s: dlopen fallito",path);
        dm_status("Modulo non caricato (vedi terminale)");
        plugin_log("FAIL",path,dlerror());
        return -1;
    }
    int(*entry)(const DmHostAPI*)=(int(*)(const DmHostAPI*))dlsym(handle,"dm_plugin_entry");
    if(!entry||entry(&dm_host_api)<0) {
        dlclose(handle);
        sceIoRemove(cache);
        snprintf(plugin_diagnostic,sizeof(plugin_diagnostic),"%.100s: entry/API incompatibile",path);
        dm_status("Modulo incompatibile o ID app gia registrato");
        plugin_log("FAIL",path,"entry/API incompatibile o ID app gia registrato");
        return -1;
    }
#else
    const DmHostAPI*api=&dm_host_api;
    int result=-1;
    int module=sceKernelLoadStartModule(cache,sizeof(api),&api,0,NULL,&result);
    if(module<0||result<0) {
        sceIoRemove(cache);
        snprintf(plugin_diagnostic,sizeof(plugin_diagnostic),"%.86s: LoadStartModule=%08X result=%08X",path,(unsigned)module,(unsigned)result);
        dm_status("Modulo Vita non caricato o API incompatibile");
        char detail[64];
        snprintf(detail,sizeof(detail),"LoadStartModule=%08X result=%08X",(unsigned)module,(unsigned)result);
        plugin_log("FAIL",path,detail);
        return -1;
    }
#endif
    /* Keep code resident; extraction files can be removed after loading. */  sceIoRemove(cache);
    DmInstalledApp*record=&loaded[loaded_count++];
    snprintf(record->path,DM_PATH_MAX,"%s",path);
    record->app=dm_registered_app(before);
    record->installed=1;
    record->bundled=!strncmp(path,"app0:",5);
    plugin_diagnostic[0]=0;
    dm_status("App aggiunta al menu Start");
    plugin_log("OK",path,record->app?record->app->id:"registrata");
    return 0;
}
int dm_load_saver(const char*path){
 static char paths[12][DM_PATH_MAX];static int count;for(int i=0;i<count;i++)if(!strcmp(paths[i],path))return 0;if(count==12)return -1;
 char cache[DM_PATH_MAX];if(extract(path,cache)<0)return -1;int before=dm_saver_count();
#ifdef DESKTOP_PREVIEW
 char file[DM_PATH_MAX+128];snprintf(file,sizeof(file),"native/desktop/demo/ux0/%s",cache+5);void*handle=dlopen(file,RTLD_NOW|RTLD_LOCAL);if(!handle){sceIoRemove(cache);return -1;}int(*entry)(const DmHostAPI*)=(int(*)(const DmHostAPI*))dlsym(handle,"dm_plugin_entry");if(!entry||entry(&dm_host_api)<0){dlclose(handle);sceIoRemove(cache);return -1;}
#else
 const DmHostAPI*api=&dm_host_api;int result=-1;int module=sceKernelLoadStartModule(cache,sizeof(api),&api,0,NULL,&result);if(module<0||result<0){sceIoRemove(cache);return -1;}
#endif
 sceIoRemove(cache);if(dm_saver_count()!=before+1)return -1;snprintf(paths[count++],DM_PATH_MAX,"%s",path);return 0;
}
void dm_scan_plugins(void) {
    plugin_log_reset();
    plugin_log("INFO",NULL,"inizio scansione app");
#ifndef DESKTOP_PREVIEW
    int net_result=sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    char net_detail[64];
    snprintf(net_detail,sizeof(net_detail),"SCE_SYSMODULE_NET load=%08X",(unsigned)net_result);
    plugin_log("INFO",NULL,net_detail);
#endif
#ifdef DESKTOP_PREVIEW
    const char*directories[]={"app0:/build/apps/","ux0:/data/desktop-mode/apps/"};
    const char*scan0="app0:/build/apps/";
#else
    const char*directories[]={"ux0:/data/desktop-mode/apps/","app0:/apps/"};
    const char*scan0=directories[0];
#endif
    for(int i=0;i<2;i++) {
        int fd=sceIoDopen(i?directories[i]:scan0);
        if(fd<0)continue;
        SceIoDirent e;
        memset(&e,0,sizeof(e));
        while(sceIoDread(fd,&e)>0) {
            const char*ext=strrchr(e.d_name,'.');
            if(ext&&!strcasecmp(ext,".dmapp")) {
                char full[DM_PATH_MAX];
#ifdef DESKTOP_PREVIEW
                const char*directory=i?directories[i]:scan0;
#else
                const char*directory=directories[i];
#endif
                if(!dm_fs_join(full,sizeof(full),directory,e.d_name)) {
                    /* Temporarily attach removed bundles to discover their metadata, then hide. */                     if(is_removed(full)&&!strncmp(full,"app0:",5)) {
                        int r=-1;
                        for(int j=0;j<removed_count;j++)if(!strcmp(removed[j],full))r=j;
                        char saved[DM_PATH_MAX];
                        snprintf(saved,sizeof(saved),"%s",removed[r]);
                        removed[r][0]=0;
                        dm_load_plugin(full);
                        snprintf(removed[r],sizeof(removed[r]),"%s",saved);
                        for(int j=0;j<loaded_count;j++)if(!strcmp(loaded[j].path,full)&&loaded[j].installed&&dm_unregister_app(loaded[j].app->id)==0)loaded[j].installed=0;
                        plugin_log("SKIP",full,"segnata come disinstallata");
                    }else if(is_removed(full))plugin_log("SKIP",full,"segnata come disinstallata");
                    else dm_load_plugin(full);
                }
            }
            memset(&e,0,sizeof(e));
        }
        sceIoDclose(fd);
    }
#ifndef DESKTOP_PREVIEW
    /* Fall back to direct paths if app0 directory enumeration is unavailable. */
    static const char*bundled[]={"notepad","counter","browser","calculator","taskmanager","paint","images","console","pdf","network","solitaire","minesweeper","media"};
    for(unsigned i=0;i<sizeof(bundled)/sizeof(bundled[0]);i++){
        char path[DM_PATH_MAX];
        int n=snprintf(path,sizeof(path),"app0:/apps/%s.dmapp",bundled[i]);
        if(n>0&&(size_t)n<sizeof(path))dm_load_plugin(path);
    }
#endif
}
int dm_plugin_list(DmInstalledApp*out,int capacity) {
    int n=loaded_count<capacity?loaded_count:capacity;
    memcpy(out,loaded,n*sizeof(*out));
    return n;
}
int dm_plugin_uninstall(const char*id) {
    for(int i=0;i<loaded_count;i++) {
        DmInstalledApp*r=&loaded[i];
        if(!r->app||strcmp(id,r->app->id)||!r->installed)continue;
        if(removed_count==DM_APP_LIMIT) {
            dm_status("Lista disinstallazioni piena");
            return -1;
        }
        /* Unregister first checks every open instance; no dirty document is discarded. */   if(dm_unregister_app(id)<0) {
            dm_status("Chiudi tutte le finestre dell'app prima di disinstallarla");
            return -1;
        }
        snprintf(removed[removed_count++],DM_PATH_MAX,"%s",r->path);
        if(write_removed()<0) {
            removed_count--;
            dm_register_app(r->app);
            dm_status("Impossibile salvare la disinstallazione");
            return -1;
        }
        if(!r->bundled&&sceIoRemove(r->path)<0) {
            removed_count--;
            write_removed();
            dm_register_app(r->app);
            dm_status("Impossibile rimuovere il pacchetto");
            return -1;
        }
        r->installed=0;
        dm_status("App disinstallata; i documenti sono conservati");
        return 0;
    }
    return -1;
}
int dm_plugin_reinstall(const char*id) {
    for(int i=0;i<loaded_count;i++) {
        DmInstalledApp*r=&loaded[i];
        if(!r->app||strcmp(id,r->app->id)||r->installed||!r->bundled)continue;
        int at=-1;
        for(int j=0;j<removed_count;j++)if(!strcmp(removed[j],r->path))at=j;
        if(at<0)return -1;
        char saved[DM_PATH_MAX];
        snprintf(saved,sizeof(saved),"%s",removed[at]);
        memmove(removed+at,removed+at+1,(removed_count-at-1)*sizeof(*removed));
        removed_count--;
        if(write_removed()<0) {
            snprintf(removed[removed_count++],DM_PATH_MAX,"%s",saved);
            return -1;
        }
        if(dm_register_app(r->app)<0) {
            snprintf(removed[removed_count++],DM_PATH_MAX,"%s",saved);
            write_removed();
            return -1;
        }
        r->installed=1;
        dm_status("App preinstallata ripristinata");
        return 0;
    }
    return -1;
}
int dm_plugin_install(const char*path) {
    read_removed();
    int at=-1;
    for(int i=0;i<removed_count;i++)if(!strcmp(removed[i],path))at=i;
    if(at<0)return dm_load_plugin(path);
    char saved[DM_PATH_MAX];
    snprintf(saved,sizeof(saved),"%s",removed[at]);
    memmove(removed+at,removed+at+1,(removed_count-at-1)*sizeof(*removed));
    removed_count--;
    if(write_removed()<0) {
        snprintf(removed[removed_count++],DM_PATH_MAX,"%s",saved);
        return -1;
    }
    int result=dm_load_plugin(path);
    if(result<0) {
        snprintf(removed[removed_count++],DM_PATH_MAX,"%s",saved);
        write_removed();
    }
    return result;
}
#ifdef DESKTOP_PREVIEW
void dm_plugin_test_settings(const char*path) {
    snprintf(removed_path,sizeof(removed_path),"%s",path?path:REMOVED);
    removed_count=removed_initialized=0;
    read_removed();
}
#endif

int dm_plugins_renamed(const char*source,const char*destination){
 read_removed();int changed[DM_APP_LIMIT]={0},marked[DM_APP_LIMIT]={0},any=0;
 for(int i=0;i<loaded_count;i++){
  char before[DM_PATH_MAX];memcpy(before,loaded[i].path,sizeof(before));before[sizeof(before)-1]=0;dm_path_renamed(loaded[i].path,sizeof(loaded[i].path),source,destination);changed[i]=strcmp(before,loaded[i].path)!=0;
 }
 for(int i=0;i<removed_count;i++){
  char before[DM_PATH_MAX];memcpy(before,removed[i],sizeof(before));before[sizeof(before)-1]=0;dm_path_renamed(removed[i],sizeof(removed[i]),source,destination);marked[i]=strcmp(before,removed[i])!=0;any|=marked[i];
 }
 if(any&&write_removed()<0){
  for(int i=0;i<loaded_count;i++)if(changed[i])dm_path_renamed(loaded[i].path,sizeof(loaded[i].path),destination,source);
  for(int i=0;i<removed_count;i++)if(marked[i])dm_path_renamed(removed[i],sizeof(removed[i]),destination,source);
  return -1;
 }
 return 0;
}
