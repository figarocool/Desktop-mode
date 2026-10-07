#include "updater.h"
#include "core_installer.h"
#include "app_manager.h"
#include "desktop_api.h"
#include <curl/curl.h>
#include <openssl/sha.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
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

static CURL *new_request(const char *url) {
    CURL *curl=curl_easy_init();
    if(!curl)return NULL;
    curl_easy_setopt(curl,CURLOPT_URL,url);
    /* Vita can expose stale system proxy settings to libcurl. GitHub is
       reachable directly on the console; bypass environment proxies and
       keep the updater on the well-tested HTTP/1.1 path. */
    curl_easy_setopt(curl,CURLOPT_PROXY,"");
    curl_easy_setopt(curl,CURLOPT_HTTP_VERSION,CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);
    curl_easy_setopt(curl,CURLOPT_MAXREDIRS,5L);
    curl_easy_setopt(curl,CURLOPT_PROTOCOLS_STR,"https");
    curl_easy_setopt(curl,CURLOPT_REDIR_PROTOCOLS_STR,"https");
    curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,8L);
    curl_easy_setopt(curl,CURLOPT_TIMEOUT,90L);
    curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(curl,CURLOPT_FAILONERROR,1L);
    curl_easy_setopt(curl,CURLOPT_USERAGENT,"DesktopMode/1 Vita updater");
#ifdef DESKTOP_PREVIEW
    curl_easy_setopt(curl,CURLOPT_CAINFO,"native/assets/cacert.pem");
#else
    curl_easy_setopt(curl,CURLOPT_CAINFO,"app0:/assets/cacert.pem");
#endif
    return curl;
}

static int fetch_manifest(MemorySink *sink) {
    CURL *curl=new_request(MANIFEST_URL);
    if(!curl)return -1;
    curl_easy_setopt(curl,CURLOPT_TIMEOUT,20L);
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,memory_write);
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,sink);
    CURLcode code=curl_easy_perform(curl);
    long status=0;curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
    curl_easy_cleanup(curl);
    return code==CURLE_OK&&status==200&&sink->size?0:-1;
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
    sceIoRemove(temporary);
    int fd=sceIoOpen(temporary,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
    if(fd<0)return -1;
    FileSink sink={fd,0,0,limit};
    CURL *curl=new_request(url);
    if(!curl){sceIoClose(fd);sceIoRemove(temporary);return -1;}
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,file_write);
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,&sink);
    CURLcode code=curl_easy_perform(curl);
    long status=0;curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
    curl_easy_cleanup(curl);
    if(sceIoClose(fd)<0)sink.failed=1;
    if(code!=CURLE_OK||status!=200||sink.failed||!sink.bytes||!digest_matches(temporary,expected)){
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
    /* VitaShell-style one-shot updater bubble is removed after it relaunches us. */
    dm_core_cleanup_updater();
    /* The boot updater runs before dm_system_info() and plugin scanning.  Load
       Vita's socket stack before libcurl creates its first handle. */
    if(dm_system_network_init()<0){result->failed=1;return;}
    static int curl_ready;
    if(!curl_ready){if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK){result->failed=1;return;}curl_ready=1;}
    /* The Vita main thread has a small stack. Keeping the 64 KiB manifest
       buffer here leaves too little room for libcurl/OpenSSL's TLS call stack
       and can corrupt the return address while checking GitHub at boot. */
    static MemorySink sink;
    memset(&sink,0,sizeof(sink));
    if(fetch_manifest(&sink)<0){result->failed=1;return;}
    char schema[8],tag[48];
    if(json_number(sink.data,"schema",schema,sizeof(schema))<0||strcmp(schema,"1")||json_string(sink.data,sink.data+sink.size,"tag",tag,sizeof(tag))<0||!tag[0]){result->failed=1;return;}
    snprintf(result->tag,sizeof(result->tag),"%s",tag);
    ensure_directories();
    const char *apps_object=object_for(sink.data,"apps");
    if(!apps_object){result->failed=1;return;}
    for(unsigned i=0;i<sizeof(builtins)/sizeof(builtins[0]);i++){
        const char *object=object_for(apps_object,builtins[i].id);
        char asset[96],digest[72],expected[96];
        snprintf(expected,sizeof(expected),"%s.dmapp",builtins[i].id);
        if(field_from_object(object,"asset",asset,sizeof(asset))<0||strcmp(asset,expected)||field_from_object(object,"sha256",digest,sizeof(digest))<0||!valid_digest(digest)){result->failed=1;continue;}
        int changed=update_app(builtins[i].id,digest);
        if(changed>0)result->apps_updated++;
        else if(changed<0)result->failed++;
    }
    char core_digest[72],core_asset[96],current_version[64]={0};
    const char *core=object_for(sink.data,"core");
    if(field_from_object(core,"asset",core_asset,sizeof(core_asset))<0||strcmp(core_asset,"desktop-mode.vpk")||field_from_object(core,"sha256",core_digest,sizeof(core_digest))<0||!valid_digest(core_digest)){result->failed++;return;}
    dm_fs_read("app0:/assets/version.txt",current_version,sizeof(current_version));
    current_version[strcspn(current_version,"\r\n")]=0;
    if(release_is_newer(tag,current_version)){
        char temp[DM_PATH_MAX],final[DM_PATH_MAX];
        snprintf(temp,sizeof(temp),UPDATE_DIR ".desktop-mode.vpk.part");
        snprintf(final,sizeof(final),UPDATE_DIR "desktop-mode.vpk");
        if(!digest_matches(final,core_digest)){
            if(download_asset(core_asset,temp,CORE_LIMIT,core_digest)<0){result->failed++;return;}
            if(replace_file(temp,final,UPDATE_DIR ".desktop-mode.vpk.old")<0){sceIoRemove(temp);result->failed++;return;}
        }
        result->core_ready=1;
        if(core_failure_for_tag(tag,&result->core_install_error))return;
        result->core_install_error=dm_core_install_vpk(final,tag,&result->core_must_exit);
        if(result->core_install_error<0)remember_core_failure(tag,result->core_install_error);
    }
#endif
}
