#include "updater.h"
#include "core_installer.h"
#include "app_manager.h"
#include "desktop_api.h"
#include <curl/curl.h>
#include <openssl/sha.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/message_dialog.h>
#include <psp2/kernel/threadmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UPDATE_DIR "ux0:/data/desktop-mode/updates/"
#define APP_DIR "ux0:/data/desktop-mode/apps/"
#define MANIFEST_URL "https://github.com/figarocool/Desktop-mode/releases/latest/download/desktop-mode-update.json"
#define MANIFEST_LIMIT (64u * 1024u)
#define CORE_LIMIT (64u * 1024u * 1024u)
#define APP_LIMIT (2u * 1024u * 1024u)

typedef struct { char data[MANIFEST_LIMIT+1]; size_t size; } MemorySink;
typedef struct { int fd; int failed; uint64_t bytes; uint64_t limit; } FileSink;
typedef struct { const char *id; } Builtin;

static const Builtin builtins[]={
    {"notepad"},{"counter"},{"browser"},{"calculator"},{"taskmanager"},
    {"paint"},{"images"},{"console"},{"pdf"},{"network"},
    {"solitaire"},{"minesweeper"},{"media"}
};
static int updater_curl_ready;
static const char *updater_stage="Avvio controllo aggiornamenti...";
static SceMsgDialogParam updater_dialog;
static SceMsgDialogProgressBarParam updater_dialog_bar;
static int updater_dialog_active;
static unsigned updater_dialog_ticks;
static char updater_dialog_description[SCE_MSG_DIALOG_USER_MSG_SIZE];
static char updater_dialog_bar_text[96];

static void update_trace(const char *stage,int code,long status,uint64_t bytes,int reset) {
    sceIoMkdir("ux0:/data",0777);
    sceIoMkdir("ux0:/data/desktop-mode",0777);
    int fd=sceIoOpen("ux0:/data/desktop-mode/update.log",SCE_O_WRONLY|SCE_O_CREAT|(reset?SCE_O_TRUNC:0),0666);
    if(fd<0)return;
    if(!reset)sceIoLseek(fd,0,SCE_SEEK_END);
    char line[160];
    int n=snprintf(line,sizeof(line),"%s code=%08X http=%ld bytes=%llu\n",stage,(unsigned)code,status,(unsigned long long)bytes);
    if(n>0&&(size_t)n<sizeof(line))sceIoWrite(fd,line,(size_t)n);
    sceIoClose(fd);
}

static void updater_dialog_close(void) {
    if(!updater_dialog_active)return;
    sceMsgDialogAbort();
    for(unsigned i=0;i<100&&sceMsgDialogGetStatus()==SCE_COMMON_DIALOG_STATUS_RUNNING;i++)sceKernelDelayThread(10000);
    SceMsgDialogResult result;
    sceMsgDialogGetResult(&result);
    sceMsgDialogTerm();
    updater_dialog_active=0;
}

static void updater_dialog_start(const char *asset) {
    updater_dialog_close();
    const char *destination=!strcmp(asset,"desktop-mode.vpk")?
        "Dopo il download aggiorna eboot.bin, app integrate e risorse in ux0:/app/DSKMODE01.":
        "Dopo la verifica, l'app sara salvata in ux0:/data/desktop-mode/apps/.";
    snprintf(updater_dialog_description,sizeof(updater_dialog_description),
        "Desktop Mode sta scaricando %s.\n\n%s\nI file personali restano invariati.",asset,destination);
    memset(&updater_dialog,0,sizeof(updater_dialog));
    memset(&updater_dialog_bar,0,sizeof(updater_dialog_bar));
    sceMsgDialogParamInit(&updater_dialog);
    updater_dialog_bar.barType=SCE_MSG_DIALOG_PROGRESSBAR_TYPE_PERCENTAGE;
    updater_dialog_bar.sysMsgParam.sysMsgType=SCE_MSG_DIALOG_SYSMSG_TYPE_INVALID;
    updater_dialog_bar.msg=(const SceChar8 *)updater_dialog_description;
    updater_dialog.mode=SCE_MSG_DIALOG_MODE_PROGRESS_BAR;
    updater_dialog.progBarParam=&updater_dialog_bar;
    int rc=sceMsgDialogInit(&updater_dialog);
    if(rc<0){update_trace("progress-dialog-init",rc,0,0,0);return;}
    updater_dialog_active=1;
    updater_dialog_ticks=0;
    sceKernelDelayThread(100000);
    sceMsgDialogProgressBarSetValue(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT,0);
    sceMsgDialogProgressBarSetMsg(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT,(const SceChar8 *)"Avvio download...");
}

static size_t memory_write(char *data,size_t size,size_t count,void *context) {
    MemorySink *sink=context;
    if(size&&count>SIZE_MAX/size)return 0;
    size_t n=size*count;
    if(n>MANIFEST_LIMIT-sink->size)return 0;
    memcpy(sink->data+sink->size,data,n);
    sink->size+=n;
    sink->data[sink->size]=0;
    return n;
}

static size_t file_write(char *data,size_t size,size_t count,void *context) {
    FileSink *sink=context;
    if(size&&count>SIZE_MAX/size)return 0;
    size_t n=size*count;
    if(n>sink->limit-sink->bytes){sink->failed=1;return 0;}
    size_t at=0;
    while(at<n){int wrote=sceIoWrite(sink->fd,data+at,n-at);if(wrote<=0){sink->failed=1;return 0;}at+=(size_t)wrote;}
    sink->bytes+=n;
    return n;
}

typedef size_t (*HttpWrite)(char *,size_t,size_t,void *);

/* Vita firmware 3.60's SceHttps cannot negotiate modern TLS with GitHub.
   Use VitaSDK's curl-mbedtls build, bundled in the VPK, and validate the
   complete certificate chain using the app's CA bundle. */
static int updater_http_init(void) {
    if(updater_curl_ready)return 0;
    CURLcode rc=curl_global_init(CURL_GLOBAL_DEFAULT);
    if(rc!=CURLE_OK){update_trace("curl-global-init",(int)rc,0,0,0);return -1;}
    updater_curl_ready=1;
    update_trace("curl-mbedtls-init",0,0,0,0);
    return 0;
}

static int updater_curl_progress(void *unused,curl_off_t download_total,curl_off_t download_now,curl_off_t upload_total,curl_off_t upload_now) {
    (void)unused;
    (void)upload_total;(void)upload_now;
    if(updater_dialog_active&&((++updater_dialog_ticks&7u)==0u||
       (download_total>0&&download_now>=download_total))){
        unsigned percent=download_total>0?(unsigned)((download_now*100)/download_total):0;
        if(percent>100)percent=100;
        sceMsgDialogProgressBarSetValue(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT,percent);
        unsigned long long now_kib=download_now>0?(unsigned long long)download_now/1024u:0;
        unsigned long long total_kib=download_total>0?(unsigned long long)download_total/1024u:0;
        if(total_kib)snprintf(updater_dialog_bar_text,sizeof(updater_dialog_bar_text),"%u%% - %llu / %llu KiB",percent,now_kib,total_kib);
        else snprintf(updater_dialog_bar_text,sizeof(updater_dialog_bar_text),"%llu KiB scaricati",now_kib);
        sceMsgDialogProgressBarSetMsg(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT,(const SceChar8 *)updater_dialog_bar_text);
    }
    return 0;
}

static int http_get(const char *url,HttpWrite write,void *context,long *status_out) {
    int init_rc=updater_http_init();
    if(init_rc<0)return init_rc;
    CURL *curl=curl_easy_init();
    if(!curl){update_trace("curl-easy-init",-1,0,0,0);return -1;}
    char error[CURL_ERROR_SIZE]={0};
    curl_easy_setopt(curl,CURLOPT_ERRORBUFFER,error);
    curl_easy_setopt(curl,CURLOPT_URL,url);
    curl_easy_setopt(curl,CURLOPT_HTTPGET,1L);
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,write);
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,context);
    curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);
    curl_easy_setopt(curl,CURLOPT_MAXREDIRS,8L);
    curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,20L);
    curl_easy_setopt(curl,CURLOPT_TIMEOUT,180L);
    curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(curl,CURLOPT_USERAGENT,"DesktopMode/1 (PS Vita updater)");
    curl_easy_setopt(curl,CURLOPT_PROTOCOLS_STR,"https");
    curl_easy_setopt(curl,CURLOPT_REDIR_PROTOCOLS_STR,"https");
    curl_easy_setopt(curl,CURLOPT_SSLVERSION,(long)CURL_SSLVERSION_TLSv1_2);
    curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);
    curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,updater_curl_progress);
    curl_easy_setopt(curl,CURLOPT_XFERINFODATA,NULL);
    curl_easy_setopt(curl,CURLOPT_CAINFO,"app0:/assets/cacert.pem");
    curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
    curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
    CURLcode rc=curl_easy_perform(curl);
    long status=0;
    curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
    if(status_out)*status_out=status;
    if(rc!=CURLE_OK){
        update_trace("curl-request",(int)rc,status,(uint64_t)strlen(error),0);
        if(error[0])update_trace(error,0,status,0,0);
    }else if(status!=200)update_trace("http-status",0,status,0,0);
    else update_trace("http-complete",0,status,0,0);
    curl_easy_cleanup(curl);
    return rc==CURLE_OK&&status==200?0:-1;
}

static int fetch_manifest(MemorySink *sink) {
    updater_stage="Controllo aggiornamenti su GitHub...";
    update_trace(updater_stage,0,0,0,0);
    long status=0;
    int rc=http_get(MANIFEST_URL,memory_write,sink,&status);
    if(rc<0||status!=200||!sink->size){update_trace("manifest-failed",rc,status,sink->size,0);return -1;}
    update_trace("manifest-ok",0,status,sink->size,0);
    return 0;
}

static int json_string(const char *begin,const char *end,const char *key,char *out,size_t cap) {
    char needle[80];int n=snprintf(needle,sizeof(needle),"\"%s\"",key);
    if(n<0||(size_t)n>=sizeof(needle))return -1;
    const char *p=begin;
    while(p<end){
        const char *found=strstr(p,needle);
        if(!found||found>=end)return -1;
        const char *colon=found+strlen(needle);
        while(colon<end&&(*colon==' '||*colon=='\t'||*colon=='\r'||*colon=='\n'))colon++;
        if(colon>=end||*colon!=':'){p=found+strlen(needle);continue;}
        colon++;
        while(colon<end&&(*colon==' '||*colon=='\t'||*colon=='\r'||*colon=='\n'))colon++;
        if(colon>=end||*colon!='"')return -1;
        const char *value=++colon;
        const char *finish=value;
        while(finish<end&&*finish!='"'){if(*finish=='\\')return -1;finish++;}
        if(finish>=end||(size_t)(finish-value)>=cap)return -1;
        memcpy(out,value,(size_t)(finish-value));out[finish-value]=0;
        return 0;
    }
    return -1;
}

static int json_number(const char *json,const char *key,char *out,size_t cap) {
    char needle[80];int n=snprintf(needle,sizeof(needle),"\"%s\"",key);
    if(n<0||(size_t)n>=sizeof(needle))return -1;
    const char *p=strstr(json,needle);
    if(!p)return -1;
    p+=strlen(needle);while(*p&&(*p==' '||*p=='\t'||*p=='\r'||*p=='\n'))p++;
    if(*p++!=':')return -1;
    while(*p&&(*p==' '||*p=='\t'||*p=='\r'||*p=='\n'))p++;
    size_t at=0;while(p[at]>='0'&&p[at]<='9')at++;
    if(!at||at>=cap)return -1;
    memcpy(out,p,at);out[at]=0;return 0;
}

static int valid_digest(const char *s) {
    if(strlen(s)!=64)return 0;
    for(int i=0;i<64;i++)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')||(s[i]>='A'&&s[i]<='F')))return 0;
    return 1;
}

typedef struct { unsigned part[3]; const char *suffix; } ReleaseVersion;

static int parse_release_version(const char *s,ReleaseVersion *v) {
    if(!s||!v)return 0;
    if(*s=='v'||*s=='V')s++;
    for(int i=0;i<3;i++){
        if(*s<'0'||*s>'9')return 0;
        unsigned value=0;
        do{unsigned digit=(unsigned)(*s-'0');if(value>(999999u-digit)/10u)return 0;value=value*10u+digit;s++;}while(*s>='0'&&*s<='9');
        v->part[i]=value;
        if(i<2){if(*s!='.')return 0;s++;}
    }
    if(*s&&*s!='-'&&*s!='+')return 0;
    v->suffix=s;
    return 1;
}

static int release_is_newer(const char *candidate,const char *installed) {
    ReleaseVersion a,b;
    /* Local development builds have no release ordering; never replace one
       with an older GitHub release just because its string differs. */
    if(!parse_release_version(candidate,&a)||!parse_release_version(installed,&b))return 0;
    for(int i=0;i<3;i++)if(a.part[i]!=b.part[i])return a.part[i]>b.part[i];
    int a_pre=*a.suffix=='-',b_pre=*b.suffix=='-';
    if(a_pre!=b_pre)return !a_pre;
    if(!a_pre)return 0;
    return strcmp(a.suffix,b.suffix)>0;
}

static const char *object_for(const char *json,const char *key) {
    char needle[80];int n=snprintf(needle,sizeof(needle),"\"%s\"",key);
    if(n<0||(size_t)n>=sizeof(needle))return NULL;
    const char *p=strstr(json,needle);
    if(!p)return NULL;
    p+=strlen(needle);while(*p&&(*p==' '||*p=='\t'||*p=='\r'||*p=='\n'))p++;
    if(*p++!=':')return NULL;
    while(*p&&(*p==' '||*p=='\t'||*p=='\r'||*p=='\n'))p++;
    return *p=='{'?p:NULL;
}

static int field_from_object(const char *object,const char *key,char *out,size_t cap) {
    if(!object||*object!='{')return -1;
    const char *end=strchr(object,'}');
    return end?json_string(object,end+1,key,out,cap):-1;
}

static int hash_path(const char *path,unsigned char digest[SHA256_DIGEST_LENGTH]) {
    int fd=sceIoOpen(path,SCE_O_RDONLY,0);
    if(fd<0)return -1;
    SHA256_CTX ctx;SHA256_Init(&ctx);
    unsigned char buffer[16384];int n;
    while((n=sceIoRead(fd,buffer,sizeof(buffer)))>0)SHA256_Update(&ctx,buffer,(size_t)n);
    sceIoClose(fd);
    if(n<0)return -1;
    SHA256_Final(digest,&ctx);return 0;
}

static void digest_hex(const unsigned char digest[SHA256_DIGEST_LENGTH],char out[65]) {
    static const char digits[]="0123456789abcdef";
    for(int i=0;i<SHA256_DIGEST_LENGTH;i++){out[i*2]=digits[digest[i]>>4];out[i*2+1]=digits[digest[i]&15];}
    out[64]=0;
}

static int digest_matches(const char *path,const char *expected) {
    unsigned char digest[SHA256_DIGEST_LENGTH];char hex[65];
    if(hash_path(path,digest)<0)return 0;
    digest_hex(digest,hex);return !dm_ascii_casecmp(hex,expected);
}

static int download_asset(const char *asset,const char *temporary,uint64_t limit,const char *expected) {
    char url[256];
    int n=snprintf(url,sizeof(url),"https://github.com/figarocool/Desktop-mode/releases/latest/download/%s",asset);
    if(n<0||(size_t)n>=sizeof(url))return -1;
    char label[128];
    if(!strcmp(asset,"desktop-mode.vpk"))snprintf(label,sizeof(label),"Scaricamento aggiornamento del sistema...");
    else snprintf(label,sizeof(label),"Scaricamento app %s...",asset);
    updater_stage=label;
    update_trace(updater_stage,0,0,0,0);
    sceIoRemove(temporary);
    int fd=sceIoOpen(temporary,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
    if(fd<0)return -1;
    FileSink sink={fd,0,0,limit};
    long status=0;
    updater_dialog_start(asset);
    int code=http_get(url,file_write,&sink,&status);
    if(updater_dialog_active){
        if(code==0)sceMsgDialogProgressBarSetValue(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT,100);
        updater_dialog_close();
    }
    if(sceIoClose(fd)<0)sink.failed=1;
    if(code<0||status!=200||sink.failed||!sink.bytes||!digest_matches(temporary,expected)){
        sceIoRemove(temporary);return -1;
    }
    return 0;
}

static int package_is_wasm(const char *path) {
    unsigned char h[32];int fd=sceIoOpen(path,SCE_O_RDONLY,0);
    if(fd<0)return 0;
    int ok=sceIoRead(fd,h,sizeof(h))==(int)sizeof(h);sceIoClose(fd);
    if(!ok||memcmp(h,"DMAPP001",8)||h[8]!=0||h[9]!=0||h[10]!=0||h[11]!=0)return 0;
    if(h[12]!=3||h[13]||h[14]||h[15]||h[20]!=1||h[21]||h[22]||h[23]||h[24]!=1||h[25]||h[26]||h[27]||h[28]||h[29]||h[30]||h[31])return 0;
    uint32_t size=(uint32_t)h[16]|(uint32_t)h[17]<<8|(uint32_t)h[18]<<16|(uint32_t)h[19]<<24;
    SceIoStat st;
    return size>0&&size<=APP_LIMIT&&sceIoGetstat(path,&st)>=0&&(uint64_t)st.st_size==(uint64_t)size+32;
}

static int replace_file(const char *temporary,const char *destination,const char *backup) {
    SceIoStat st;int exists=sceIoGetstat(destination,&st)>=0;
    sceIoRemove(backup);
    if(exists&&sceIoRename(destination,backup)<0)return -1;
    if(sceIoRename(temporary,destination)<0){if(exists)sceIoRename(backup,destination);return -1;}
    if(exists)sceIoRemove(backup);
    return 0;
}

static int ensure_directories(void) {
    sceIoMkdir("ux0:/data",0777);
    sceIoMkdir("ux0:/data/desktop-mode",0777);
    sceIoMkdir("ux0:/data/desktop-mode/apps",0777);
    sceIoMkdir("ux0:/data/desktop-mode/updates",0777);
    return 0;
}

static int core_failure_for_tag(const char *tag,int *error) {
    char data[96]={0};
    int fd=sceIoOpen(UPDATE_DIR ".core-install-failed",SCE_O_RDONLY,0);
    if(fd<0)return 0;
    int n=sceIoRead(fd,data,sizeof(data)-1);sceIoClose(fd);
    if(n<=0)return 0;
    data[n]=0;
    char saved[48]={0};unsigned code=0;
    if(sscanf(data,"%47s %x",saved,&code)!=2||strcmp(saved,tag))return 0;
    if(error)*error=(int)code;
    return 1;
}

static void remember_core_failure(const char *tag,int error) {
    int fd=sceIoOpen(UPDATE_DIR ".core-install-failed",SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
    if(fd<0)return;
    char data[96];int n=snprintf(data,sizeof(data),"%s %08X\n",tag,(unsigned)error);
    if(n>0&&(size_t)n<sizeof(data))sceIoWrite(fd,data,(size_t)n);
    sceIoClose(fd);
}

static int update_app(const char *id,const char *digest) {
    char user[DM_PATH_MAX],bundle[DM_PATH_MAX],temp[DM_PATH_MAX],backup[DM_PATH_MAX],asset[96];
    snprintf(user,sizeof(user),APP_DIR "%s.dmapp",id);
    snprintf(bundle,sizeof(bundle),"app0:/apps/%s.dmapp",id);
    if(dm_plugin_bundle_removed(id)&&sceIoGetstat(user,&(SceIoStat){0})<0)return 0;
    if(digest_matches(user,digest)||digest_matches(bundle,digest))return 0;
    snprintf(asset,sizeof(asset),"%s.dmapp",id);
    snprintf(temp,sizeof(temp),UPDATE_DIR ".%s.dmapp.part",id);
    snprintf(backup,sizeof(backup),UPDATE_DIR ".%s.dmapp.old",id);
    if(download_asset(asset,temp,APP_LIMIT+32,digest)<0)return -1;
    if(!package_is_wasm(temp)){sceIoRemove(temp);return -1;}
    if(replace_file(temp,user,backup)<0){sceIoRemove(temp);return -1;}
    return 1;
}

void dm_updates_check(DmUpdateResult *result) {
    if(!result)return;
    memset(result,0,sizeof(*result));
#ifdef DESKTOP_PREVIEW
    return;
#else
    update_trace("startup",0,0,0,1);
    updater_stage="Avvio controllo aggiornamenti...";
    /* Do not enter vita2d drawing while boot-time update/network code runs.
       The prior splash rendered from this path caused a data abort on 3.60. */
    ensure_directories();
    /* VitaShell-style one-shot updater bubble is removed after it relaunches us. */
    dm_core_cleanup_updater();
    /* The boot updater runs before system info and plugin scanning. */
    updater_stage="Connessione alla rete...";
    update_trace(updater_stage,0,0,0,0);
    int network_rc=dm_system_network_init();
    if(network_rc<0){result->failed=1;update_trace("network-init",network_rc,0,0,0);return;}
    if(updater_http_init()<0){result->failed=1;return;}
    /* Keep the manifest buffer off the Vita main thread's small stack. */
    static MemorySink sink;
    memset(&sink,0,sizeof(sink));
    if(fetch_manifest(&sink)<0){result->failed=1;return;}
    char schema[8],tag[48];
    if(json_number(sink.data,"schema",schema,sizeof(schema))<0||strcmp(schema,"1")||json_string(sink.data,sink.data+sink.size,"tag",tag,sizeof(tag))<0||!tag[0]){result->failed=1;return;}
    snprintf(result->tag,sizeof(result->tag),"%s",tag);
    update_trace("release-manifest",0,200,sink.size,0);
    const char *apps_object=object_for(sink.data,"apps");
    if(!apps_object){result->failed=1;update_trace("manifest-apps-missing",-1,200,sink.size,0);return;}
    for(unsigned i=0;i<sizeof(builtins)/sizeof(builtins[0]);i++){
        const char *object=object_for(apps_object,builtins[i].id);
        char asset[96],digest[72],expected[96];
        snprintf(expected,sizeof(expected),"%s.dmapp",builtins[i].id);
        if(field_from_object(object,"asset",asset,sizeof(asset))<0||strcmp(asset,expected)||field_from_object(object,"sha256",digest,sizeof(digest))<0||!valid_digest(digest)){result->failed=1;continue;}
        int changed=update_app(builtins[i].id,digest);
        if(changed>0)result->apps_updated++;
        else if(changed<0){result->failed++;update_trace("app-download-failed",-1,0,0,0);}
    }
    char core_digest[72],core_asset[96],current_version[64]={0};
    const char *core=object_for(sink.data,"core");
    if(field_from_object(core,"asset",core_asset,sizeof(core_asset))<0||strcmp(core_asset,"desktop-mode.vpk")||field_from_object(core,"sha256",core_digest,sizeof(core_digest))<0||!valid_digest(core_digest)){result->failed++;update_trace("manifest-core-invalid",-1,200,sink.size,0);return;}
    dm_fs_read("app0:/assets/version.txt",current_version,sizeof(current_version));
    current_version[strcspn(current_version,"\r\n")]=0;
    if(release_is_newer(tag,current_version)){
        char temp[DM_PATH_MAX],final[DM_PATH_MAX];
        snprintf(temp,sizeof(temp),UPDATE_DIR ".desktop-mode.vpk.part");
        snprintf(final,sizeof(final),UPDATE_DIR "desktop-mode.vpk");
        if(!digest_matches(final,core_digest)){
            if(download_asset(core_asset,temp,CORE_LIMIT,core_digest)<0){result->failed++;update_trace("core-download-failed",-1,0,0,0);return;}
            if(replace_file(temp,final,UPDATE_DIR ".desktop-mode.vpk.old")<0){sceIoRemove(temp);result->failed++;update_trace("core-store-failed",-1,0,0,0);return;}
        }
        result->core_ready=1;
        if(core_failure_for_tag(tag,&result->core_install_error)){update_trace("core-previous-install-failure",result->core_install_error,0,0,0);return;}
        result->core_install_error=dm_core_install_vpk(final,tag,&result->core_must_exit);
        update_trace("core-install",result->core_install_error,0,0,0);
        if(result->core_install_error<0)remember_core_failure(tag,result->core_install_error);
    }
#endif
}
