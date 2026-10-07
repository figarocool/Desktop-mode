#include "desktop_api.h"
#include "app_runtime.h"
#include "wasm_app_runtime.h"
#include "app-sdk/wasm_app.h"
#include "network_service.h"
#include "vendor/wasm3/wasm3.h"
#include "vendor/wasm3/m3_env.h"
#include "apps/pdf_engine.h"
#include "media_player.h"
#include "vendor/browser-engine/duktape/src/duktape.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#define WASM_APP_LIMIT DM_APP_LIMIT
#define WASM_MODULE_LIMIT (2u * 1024u * 1024u)
#define WASM_MEMORY_LIMIT (4u * 1024u * 1024u)
#define WASM_FUEL_LIMIT 1000000u
#define WASM_BROWSER_FUEL_LIMIT 8000000u
#define BROWSER_JS_HEAP_LIMIT (2u * 1024u * 1024u)
#define BROWSER_JS_OUTPUT_LIMIT 2048u
static uint32_t fuel_remaining;
static int fuel_active;
static uint32_t wasm_fuel_for(IM3Function fn);
static int wasm_client_width(DmWindow *w) {
    if(!w)return 0;
    float scale=dm_ui_scale();if(scale<0.1f)scale=0.1f;
    int width=(int)(w->w/scale+0.5f);
    return width>0?width:0;
}
static int wasm_client_height(DmWindow *w) {
    if(!w)return 0;
    float scale=dm_ui_scale();if(scale<0.1f)scale=0.1f;
    int chrome=(int)(64.0f*scale+0.5f);
    int height=(int)((w->h-chrome-8)/scale+0.5f);
    return height>0?height:0;
}
M3Result m3_Yield(void) {
    if(!fuel_active)return m3Err_none;
    if(!fuel_remaining)return m3Err_trapExit;
    fuel_remaining--;
    return m3Err_none;
}
static M3Result wasm_call_v(IM3Function fn) {
    fuel_remaining=wasm_fuel_for(fn);fuel_active=1;
    M3Result error=m3_CallV(fn);fuel_active=0;return error;
}
static M3Result wasm_call_argv(IM3Function fn,uint32_t argc,const char *argv[]) {
    fuel_remaining=wasm_fuel_for(fn);fuel_active=1;
    M3Result error=m3_CallArgv(fn,argc,argv);fuel_active=0;return error;
}

typedef struct {
    DmApp app;
    char id[64], title[96];
    uint8_t *bytes;
    size_t size;
} WasmApp;
typedef struct BrowserJsBlock {struct BrowserJsBlock*next;size_t size;} BrowserJsBlock;
typedef struct {size_t used;int fuel;BrowserJsBlock*blocks;char output[BROWSER_JS_OUTPUT_LIMIT];} BrowserJsHeap;
typedef struct {
    IM3Environment env;
    IM3Runtime runtime;
    IM3Function abi, init, draw, click, close, argument_buffer;
    IM3Function text_buffer,file_buffer,file_result,text,key,tick,event,menu,dirty,discard;
    void *images[8];
    uint32_t pending_file_offset,pending_file_capacity;
    uint32_t pending_text_offset,pending_text_capacity;
    CURLM *http_multi;CURL *http_easy;char *http_body,*http_request_body;size_t http_size,http_request_size;int http_done,http_error,http_delivered;char http_error_text[CURL_ERROR_SIZE],http_effective_url[2048];
    PdfEngine *pdf_engine;unsigned char *pdf_data;
    duk_context *js;BrowserJsHeap js_heap;int js_failed;
    int failed,menu_open,close_prompted;
    char failure_message[160];
} WasmInstance;
static BrowserJsHeap *active_js_heap;
static jmp_buf browser_js_fatal_jump;static int browser_js_fatal_guard;
int dm_browser_js_timeout(void *udata){BrowserJsHeap*h=udata;if(!h)return 1;if(h->fuel<=0)return 1;h->fuel--;return 0;}
static void browser_js_free(void *udata,void *ptr);
static void *browser_js_alloc(void *udata,duk_size_t size){BrowserJsHeap*h=udata;if(!size||size>BROWSER_JS_HEAP_LIMIT-sizeof(BrowserJsBlock)||h->used>BROWSER_JS_HEAP_LIMIT-size-sizeof(BrowserJsBlock))return NULL;BrowserJsBlock*b=malloc(sizeof(*b)+size);if(!b)return NULL;b->size=size;b->next=h->blocks;h->blocks=b;h->used+=sizeof(*b)+size;return b+1;}
static void *browser_js_realloc(void *udata,void *ptr,duk_size_t size){BrowserJsHeap*h=udata;if(!ptr)return browser_js_alloc(udata,size);BrowserJsBlock*old=(BrowserJsBlock*)ptr-1;size_t old_size=old->size;if(!size){browser_js_free(udata,ptr);return NULL;}if(size>BROWSER_JS_HEAP_LIMIT-sizeof(BrowserJsBlock)||h->used-sizeof(*old)-old_size>BROWSER_JS_HEAP_LIMIT-size-sizeof(*old))return NULL;BrowserJsBlock**link=&h->blocks;while(*link&&*link!=old)link=&(*link)->next;if(!*link)return NULL;BrowserJsBlock*b=realloc(old,sizeof(*b)+size);if(!b)return NULL;*link=b;b->size=size;h->used=h->used-sizeof(*b)-old_size+sizeof(*b)+size;return b+1;}
static void browser_js_free(void *udata,void *ptr){BrowserJsHeap*h=udata;if(ptr){BrowserJsBlock*b=(BrowserJsBlock*)ptr-1;BrowserJsBlock**link=&h->blocks;while(*link&&*link!=b)link=&(*link)->next;if(*link){*link=b->next;if(h->used>=sizeof(*b)+b->size)h->used-=sizeof(*b)+b->size;}free(b);}}
static void browser_js_reclaim(BrowserJsHeap*h){while(h->blocks){BrowserJsBlock*b=h->blocks;h->blocks=b->next;free(b);}h->used=0;}
static void browser_js_fatal(void *udata,const char *message){(void)udata;(void)message;if(browser_js_fatal_guard)longjmp(browser_js_fatal_jump,1);abort();}
static void browser_js_append(const char *text,size_t n){if(!active_js_heap||!text)return;size_t used=strlen(active_js_heap->output);if(n>BROWSER_JS_OUTPUT_LIMIT-1-used)n=BROWSER_JS_OUTPUT_LIMIT-1-used;memcpy(active_js_heap->output+used,text,n);active_js_heap->output[used+n]=0;}
static duk_ret_t browser_js_write(duk_context *ctx){duk_size_t n=0;const char*s=duk_safe_to_lstring(ctx,0,&n);browser_js_append(s,(size_t)n);return 0;}
static duk_ret_t browser_js_log(duk_context *ctx){for(duk_idx_t i=0;i<duk_get_top(ctx);i++){if(i)browser_js_append(" ",1);duk_size_t n=0;const char*s=duk_safe_to_lstring(ctx,i,&n);browser_js_append(s,(size_t)n);}browser_js_append("\n",1);return 0;}
static duk_context *browser_js_create(BrowserJsHeap*h){memset(h,0,sizeof(*h));if(setjmp(browser_js_fatal_jump)){browser_js_fatal_guard=0;return NULL;}browser_js_fatal_guard=1;duk_context*ctx=duk_create_heap(browser_js_alloc,browser_js_realloc,browser_js_free,h,browser_js_fatal);browser_js_fatal_guard=0;if(!ctx)return NULL;duk_push_object(ctx);duk_push_c_function(ctx,browser_js_write,DUK_VARARGS);duk_put_prop_string(ctx,-2,"write");duk_put_global_string(ctx,"document");duk_push_object(ctx);duk_push_c_function(ctx,browser_js_log,DUK_VARARGS);duk_put_prop_string(ctx,-2,"log");duk_put_global_string(ctx,"console");duk_push_c_function(ctx,browser_js_log,DUK_VARARGS);duk_put_global_string(ctx,"print");return ctx;}
static WasmApp apps[WASM_APP_LIMIT];
static unsigned app_count;
static WasmApp *app_for(const DmWindow *w);
static void wasm_key(DmWindow *w,int key);

static WasmApp *app_for(const DmWindow *w) {
    for (unsigned i=0;i<app_count;i++) if (&apps[i].app==w->app) return &apps[i];
    return NULL;
}
static uint32_t wasm_fuel_for(IM3Function fn) {
    if(fn&&fn->module&&fn->module->runtime){
        DmWindow*w=m3_GetUserData((IM3Runtime)fn->module->runtime);
        WasmApp*app=app_for(w);
        if(app&&!strcmp(app->id,"browser"))return WASM_BROWSER_FUEL_LIMIT;
    }
    return WASM_FUEL_LIMIT;
}
static void failure(DmWindow *w, const char *operation, M3Result error) {
    WasmInstance *s=w->state;
    s->failed=1;
    char message[160];
    snprintf(message,sizeof(message),"App %.28s: %.95s",operation,error?error:"errore sconosciuto");
    snprintf(s->failure_message,sizeof(s->failure_message),"%s",message);
    dm_status(message);
    WasmApp *app=app_for(w);if(app&&!strcmp(app->id,"network"))dm_network_service_close(w);
}
static const void *host_rect(IM3Runtime runtime, IM3ImportContext ctx, uint64_t *sp, void *mem) {
    (void)ctx; (void)mem;
    DmWindow *w=m3_GetUserData(runtime);
    int32_t x=(int32_t)sp[0], y=(int32_t)sp[1], width=(int32_t)sp[2], height=(int32_t)sp[3];
    uint32_t color=(uint32_t)sp[4];
    if (w && w->used && width>0 && height>0 && width<=4096 && height<=4096) {
        int max_width=wasm_client_width(w),max_height=wasm_client_height(w);
        int64_t left=x<0?0:x, top=y<0?0:y;
        int64_t right=(int64_t)x+width, bottom=(int64_t)y+height;
        if(right>max_width)right=max_width;
        if(bottom>max_height)bottom=max_height;
        if(right>left&&bottom>top)dm_rect(w->x+(int)left,w->y+64+(int)top,(int)(right-left),(int)(bottom-top),color);
    }
    return m3Err_none;
}
static const void *host_scrollbar_draw(IM3Runtime runtime,IM3ImportContext ctx,uint64_t*sp,void*mem){
    (void)ctx;(void)mem;DmWindow*w=m3_GetUserData(runtime);
    int x=(int32_t)sp[0],y=(int32_t)sp[1],length=(int32_t)sp[2],thickness=(int32_t)sp[3],vertical=(int32_t)sp[4],content=(int32_t)sp[5],viewport=(int32_t)sp[6],offset=(int32_t)sp[7];
    if(w&&w->used&&length>0&&length<=8192&&thickness>0&&thickness<=64&&content>0&&viewport>0&&viewport<=8192)dm_scrollbar_draw(w->x+x,w->y+64+y,length,thickness,vertical,content,viewport,offset);
    return m3Err_none;
}
static const void *host_text(IM3Runtime runtime, IM3ImportContext ctx, uint64_t *sp, void *mem) {
    (void)ctx;
    DmWindow *w=m3_GetUserData(runtime);
    uint32_t offset=(uint32_t)sp[2], memory_size=m3_GetMemorySize(runtime);
    int32_t x=(int32_t)sp[0], y=(int32_t)sp[1]; uint32_t color=(uint32_t)sp[3];
    if (!w || !w->used || offset>=memory_size) return m3Err_trapOutOfBoundsMemoryAccess;
    const char *text=(const char *)mem+offset;
    size_t left=memory_size-offset, length=0;
    while(length<left && length<512 && text[length]) length++;
    if(length==left || length==512) return m3Err_trapOutOfBoundsMemoryAccess;
    char safe[513]; memcpy(safe,text,length); safe[length]=0;
    if(x>=0&&x<wasm_client_width(w)&&y>=0&&y<=wasm_client_height(w))dm_text_raw(w->x+x,w->y+64+y,safe,color);
    return m3Err_none;
}
static const void *host_text_scaled(IM3Runtime runtime,IM3ImportContext ctx,uint64_t*sp,void*mem){
    (void)ctx;DmWindow*w=m3_GetUserData(runtime);uint32_t offset=(uint32_t)sp[2],memory_size=m3_GetMemorySize(runtime);int32_t x=(int32_t)sp[0],y=(int32_t)sp[1],percent=(int32_t)sp[4];uint32_t color=(uint32_t)sp[3];
    if(!w||!w->used||offset>=memory_size)return m3Err_trapOutOfBoundsMemoryAccess;const char*text=(const char*)mem+offset;size_t left=memory_size-offset,length=0;while(length<left&&length<512&&text[length])length++;if(length==left||length==512)return m3Err_trapOutOfBoundsMemoryAccess;char safe[513];memcpy(safe,text,length);safe[length]=0;if(x>=0&&x<wasm_client_width(w)&&y>=0&&y<=wasm_client_height(w))dm_text_raw_scaled(w->x+x,w->y+64+y,safe,color,percent);return m3Err_none;
}
static const void *host_emscripten_memcpy_big(IM3Runtime runtime,IM3ImportContext ctx,uint64_t*sp,void*mem){
    (void)ctx;int32_t*ret=(int32_t*)sp++;uint32_t dst=(uint32_t)*sp++,src=(uint32_t)*sp++,length=(uint32_t)*sp++,total=m3_GetMemorySize(runtime);
    if(dst>total||src>total||length>total-dst||length>total-src)return m3Err_trapOutOfBoundsMemoryAccess;
    memmove((uint8_t*)mem+dst,(uint8_t*)mem+src,length);*ret=(int32_t)dst;return m3Err_none;
}
static const void *host_emscripten_resize_heap(IM3Runtime runtime,IM3ImportContext ctx,uint64_t*sp,void*mem){
    (void)ctx;(void)mem;int32_t*ret=(int32_t*)sp++;uint32_t requested=(uint32_t)*sp++,current=m3_GetMemorySize(runtime);
    if(requested<=current){*ret=1;return m3Err_none;}
    if(!runtime->memoryLimit||requested>runtime->memoryLimit){*ret=0;return m3Err_none;}
    uint32_t pages=(requested+65535u)/65536u;M3Result error=ResizeMemory(runtime,pages);*ret=error?0:1;return m3Err_none;
}
static const void *host_task_count(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){
    (void)c;(void)mem;(void)r;int32_t *ret=(int32_t*)sp++;DmTaskInfo tasks[DM_MAX_WINDOWS];*ret=dm_tasks(tasks,DM_MAX_WINDOWS);return m3Err_none;
}
static int task_snapshot(DmWindow *owner,DmTaskInfo tasks[DM_MAX_WINDOWS],int index){
    (void)owner;int count=dm_tasks(tasks,DM_MAX_WINDOWS);if(index<0||index>=count)return -1;
    return count;
}
static const void *host_task_get(IM3Runtime runtime,IM3ImportContext ctx,uint64_t *sp,void *mem){
    (void)ctx;int32_t *ret=(int32_t*)sp++;int index=(int32_t)*sp++;uint32_t offset=(uint32_t)*sp++,capacity=(uint32_t)*sp++;
    uint32_t size=m3_GetMemorySize(runtime);if(offset>size||capacity>size-offset||capacity<2||capacity>256)return m3Err_trapOutOfBoundsMemoryAccess;
    DmWindow *owner=m3_GetUserData(runtime);DmTaskInfo tasks[DM_MAX_WINDOWS];int count=task_snapshot(owner,tasks,index);
    if(count<0){*ret=count;return m3Err_none;}
    const char *name=dm_ui_translate(tasks[index].title);size_t n=strlen(name);if(n>=capacity)n=capacity-1;
    memcpy((uint8_t*)mem+offset,name,n);((char*)mem)[offset+n]=0;*ret=tasks[index].minimized?1:0;return m3Err_none;
}
static const void *host_task_memory(IM3Runtime runtime,IM3ImportContext ctx,uint64_t *sp,void *mem){
    (void)ctx;(void)mem;int32_t *ret=(int32_t*)sp++;int index=(int32_t)*sp++;DmWindow *owner=m3_GetUserData(runtime);
    DmTaskInfo tasks[DM_MAX_WINDOWS];int count=task_snapshot(owner,tasks,index);if(count<0){*ret=-1;return m3Err_none;}
    uint64_t kb=dm_app_memory(tasks[index].window)/1024;*ret=kb>INT32_MAX?INT32_MAX:(int32_t)kb;return m3Err_none;
}
static const void *host_task_action(IM3Runtime runtime,IM3ImportContext ctx,uint64_t *sp,void *mem){
    (void)ctx;(void)mem;int32_t *ret=(int32_t*)sp++;int index=(int32_t)*sp++,action=(int32_t)*sp++;DmWindow *owner=m3_GetUserData(runtime);
    DmTaskInfo tasks[DM_MAX_WINDOWS];int count=task_snapshot(owner,tasks,index);if(count<0){*ret=-1;return m3Err_none;}
    DmWindow *target=tasks[index].window;
    if(action==2&&target==owner){*ret=-1;return m3Err_none;}
    if(action==0)dm_focus(target);else if(action==1)target->minimized=1;else if(action==2)dm_close(target);else{*ret=-1;return m3Err_none;}
    *ret=0;return m3Err_none;
}
static const void *host_image_load(IM3Runtime runtime,IM3ImportContext ctx,uint64_t *sp,void *mem){
    (void)ctx;int32_t *ret=(int32_t*)sp++;uint32_t offset=(uint32_t)*sp++,size=m3_GetMemorySize(runtime);
    if(offset>=size)return m3Err_trapOutOfBoundsMemoryAccess;
    const char *source=(const char*)mem+offset;size_t n=0;while(offset+n<size&&n<DM_PATH_MAX&&source[n])n++;
    if(offset+n>=size||n>=DM_PATH_MAX)return m3Err_trapOutOfBoundsMemoryAccess;
    WasmInstance *s=((DmWindow*)m3_GetUserData(runtime))->state;int slot=-1;
    for(int i=0;i<8;i++)if(!s->images[i]){slot=i;break;}
    if(slot<0){*ret=-1;return m3Err_none;}
    void *image=dm_image_load(source);if(!image){*ret=-1;return m3Err_none;}
    s->images[slot]=image;*ret=slot+1;return m3Err_none;
}
static const void *host_image_size(IM3Runtime runtime,IM3ImportContext ctx,uint64_t *sp,void *mem){
    (void)ctx;int32_t *ret=(int32_t*)sp++;int32_t handle=(int32_t)*sp++;uint32_t width_offset=(uint32_t)*sp++,height_offset=(uint32_t)*sp++,size=m3_GetMemorySize(runtime);
    if(width_offset>size-4||height_offset>size-4)return m3Err_trapOutOfBoundsMemoryAccess;
    DmWindow *w=m3_GetUserData(runtime);WasmInstance *s=w->state;if(handle<1||handle>8||!s->images[handle-1]){*ret=-1;return m3Err_none;}
    unsigned width=0,height=0;dm_image_size(s->images[handle-1],&width,&height);memcpy((uint8_t*)mem+width_offset,&width,4);memcpy((uint8_t*)mem+height_offset,&height,4);*ret=0;return m3Err_none;
}
static const void *host_image_read(IM3Runtime runtime,IM3ImportContext ctx,uint64_t *sp,void *mem){
    (void)ctx;int32_t*ret=(int32_t*)sp++;int32_t handle=(int32_t)*sp++;uint32_t offset=(uint32_t)*sp++,capacity=(uint32_t)*sp++,size=m3_GetMemorySize(runtime);
    if(offset>size||capacity>size-offset)return m3Err_trapOutOfBoundsMemoryAccess;
    DmWindow*w=m3_GetUserData(runtime);WasmInstance*s=w->state;
    *ret=handle>=1&&handle<=8&&s->images[handle-1]?dm_image_read(s->images[handle-1],(uint32_t*)((uint8_t*)mem+offset),capacity):-1;return m3Err_none;
}
static const void *host_image_draw(IM3Runtime runtime,IM3ImportContext ctx,uint64_t *sp,void *mem){
    (void)ctx;(void)mem;DmWindow *w=m3_GetUserData(runtime);WasmInstance *s=w->state;
    int32_t handle=(int32_t)sp[0],x=(int32_t)sp[1],y=(int32_t)sp[2],width=(int32_t)sp[3],height=(int32_t)sp[4];
    if(handle>=1&&handle<=8&&s->images[handle-1]&&width>0&&height>0&&width<=32768&&height<=32768)dm_image_draw_clipped(s->images[handle-1],w->x+x,w->y+64+y,width,height,w->x,w->y+64,wasm_client_width(w),wasm_client_height(w));
    return m3Err_none;
}
static const void *host_image_draw_clipped(IM3Runtime runtime,IM3ImportContext ctx,uint64_t *sp,void *mem){
    (void)ctx;(void)mem;DmWindow *w=m3_GetUserData(runtime);WasmInstance *s=w->state;
    int32_t handle=(int32_t)sp[0],x=(int32_t)sp[1],y=(int32_t)sp[2],width=(int32_t)sp[3],height=(int32_t)sp[4],cx=(int32_t)sp[5],cy=(int32_t)sp[6],cw=(int32_t)sp[7],ch=(int32_t)sp[8];
    if(handle>=1&&handle<=8&&s->images[handle-1]&&width>0&&height>0&&width<=32768&&height<=32768&&cw>0&&ch>0){int left=cx<0?0:cx,top=cy<0?0:cy,right=cx+cw,bottom=cy+ch,maxw=wasm_client_width(w),maxh=wasm_client_height(w);if(right>maxw)right=maxw;if(bottom>maxh)bottom=maxh;if(right>left&&bottom>top)dm_image_draw_clipped(s->images[handle-1],w->x+x,w->y+64+y,width,height,w->x+left,w->y+64+top,right-left,bottom-top);}
    return m3Err_none;
}
static const void *host_image_free(IM3Runtime runtime,IM3ImportContext ctx,uint64_t *sp,void *mem){
    (void)ctx;(void)mem;DmWindow *w=m3_GetUserData(runtime);WasmInstance *s=w->state;int32_t handle=(int32_t)sp[0];
    if(handle>=1&&handle<=8&&s->images[handle-1]){dm_image_free(s->images[handle-1]);s->images[handle-1]=NULL;}return m3Err_none;
}
static int guest_string(IM3Runtime runtime,void *mem,uint32_t offset,uint32_t limit,char *out,size_t capacity);
static const void *host_image_create(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){
    (void)c;int32_t *ret=(int32_t*)sp++;uint32_t w=(uint32_t)*sp++,h=(uint32_t)*sp++,pixels=(uint32_t)*sp++,total=m3_GetMemorySize(r);
    if(!w||!h||w>1024||h>1024||(uint64_t)w*h*4>total||pixels>total-(uint64_t)w*h*4)return m3Err_trapOutOfBoundsMemoryAccess;
    DmWindow *window=m3_GetUserData(r);WasmInstance *s=window->state;int slot=-1;for(int i=0;i<8;i++)if(!s->images[i]){slot=i;break;}
    if(slot<0){*ret=-1;return m3Err_none;}void *image=dm_image_create(w,h,(uint32_t*)((uint8_t*)mem+pixels));if(!image){*ret=-1;return m3Err_none;}s->images[slot]=image;*ret=slot+1;return m3Err_none;
}
static const void *host_image_update(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){
    (void)c;DmWindow *window=m3_GetUserData(r);WasmInstance *s=window->state;int32_t handle=(int32_t)sp[0];uint32_t pixels=(uint32_t)sp[1],total=m3_GetMemorySize(r);if(handle<1||handle>8||!s->images[handle-1])return m3Err_none;
    unsigned w=0,h=0;dm_image_size(s->images[handle-1],&w,&h);uint64_t bytes=(uint64_t)w*h*4;if(pixels>total||bytes>total-pixels)return m3Err_trapOutOfBoundsMemoryAccess;dm_image_update(s->images[handle-1],(uint32_t*)((uint8_t*)mem+pixels));return m3Err_none;
}
static const void *host_image_save_png(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){
    (void)c;int32_t *ret=(int32_t*)sp++;uint32_t pathoff=(uint32_t)*sp++,w=(uint32_t)*sp++,h=(uint32_t)*sp++,pixels=(uint32_t)*sp++,exclusive=(uint32_t)*sp++,total=m3_GetMemorySize(r);uint64_t bytes=(uint64_t)w*h*4;
    if(!w||!h||w>1024||h>1024||pixels>total||bytes>total-pixels)return m3Err_trapOutOfBoundsMemoryAccess;
    char path[DM_PATH_MAX];if(guest_string(r,mem,pathoff,DM_PATH_MAX,path,sizeof(path))<0){*ret=-1;return m3Err_none;}
    *ret=dm_image_save_png(path,w,h,(uint32_t*)((uint8_t*)mem+pixels),exclusive!=0);return m3Err_none;
}
static const void *host_pointer_state(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){
    (void)c;int32_t *ret=(int32_t*)sp++;uint32_t xo=(uint32_t)*sp++,yo=(uint32_t)*sp++,ho=(uint32_t)*sp++,total=m3_GetMemorySize(r);if(xo>total-4||yo>total-4||ho>total-4)return m3Err_trapOutOfBoundsMemoryAccess;DmPointerState state;dm_pointer_state(m3_GetUserData(r),&state);state.y-=64;memcpy((uint8_t*)mem+xo,&state.x,4);memcpy((uint8_t*)mem+yo,&state.y,4);memcpy((uint8_t*)mem+ho,&state.held,4);*ret=state.focused;return m3Err_none;
}
static int guest_string(IM3Runtime runtime,void *mem,uint32_t offset,uint32_t limit,char *out,size_t capacity){
    uint32_t total=m3_GetMemorySize(runtime);if(offset>=total||!capacity)return -1;
    const char *p=(const char*)mem+offset;size_t n=0;
    while(offset+n<total&&n<limit&&n+1<capacity&&p[n])n++;
    if(offset+n>=total||n==limit||n+1==capacity)return -1;
    memcpy(out,p,n);out[n]=0;return (int)n;
}
static const void *host_text_width(IM3Runtime runtime,IM3ImportContext ctx,uint64_t*sp,void*mem){
    (void)ctx;int32_t*ret=(int32_t*)sp++;char text[513];
    if(guest_string(runtime,mem,(uint32_t)sp[0],512,text,sizeof(text))<0)return m3Err_trapOutOfBoundsMemoryAccess;
    *ret=dm_text_width_raw(text);return m3Err_none;
}
static const void *host_text_width_scaled(IM3Runtime runtime,IM3ImportContext ctx,uint64_t*sp,void*mem){
    (void)ctx;int32_t*ret=(int32_t*)sp++;char text[513];if(guest_string(runtime,mem,(uint32_t)sp[0],512,text,sizeof(text))<0)return m3Err_trapOutOfBoundsMemoryAccess;int percent=(int32_t)sp[1];*ret=dm_text_width_raw_scaled(text,percent);return m3Err_none;
}
static const void *host_fs_read(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){
    (void)c;int32_t *ret=(int32_t*)sp++;uint32_t pathoff=(uint32_t)*sp++,dest=(uint32_t)*sp++,cap=(uint32_t)*sp++,total=m3_GetMemorySize(r);
    if(dest>total||cap>total-dest||cap>DM_TEXT_MAX)return m3Err_trapOutOfBoundsMemoryAccess;
    char path[DM_PATH_MAX];if(guest_string(r,mem,pathoff,DM_PATH_MAX,path,sizeof(path))<0){*ret=-1;return m3Err_none;}
    *ret=dm_fs_read(path,(char*)mem+dest,cap);return m3Err_none;
}
static const void *host_fs_write(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){
    (void)c;int32_t *ret=(int32_t*)sp++;uint32_t pathoff=(uint32_t)*sp++,source=(uint32_t)*sp++,length=(uint32_t)*sp++,exclusive=(uint32_t)*sp++,total=m3_GetMemorySize(r);
    DmWindow*w=m3_GetUserData(r);WasmApp*app=app_for(w);uint32_t limit=app&&!strcmp(app->id,"browser")?DM_WASM_HTTP_MAX_BODY:DM_TEXT_MAX;
    if(source>total||length>total-source||length>limit)return m3Err_trapOutOfBoundsMemoryAccess;
    char path[DM_PATH_MAX];if(guest_string(r,mem,pathoff,DM_PATH_MAX,path,sizeof(path))<0){*ret=-1;return m3Err_none;}
    *ret=dm_fs_write(path,(char*)mem+source,length,exclusive!=0);return m3Err_none;
}
static const void *host_fs_list_text(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){
    (void)c;int32_t *ret=(int32_t*)sp++;uint32_t pathoff=(uint32_t)*sp++,dest=(uint32_t)*sp++,cap=(uint32_t)*sp++,offset=(uint32_t)*sp++,total=m3_GetMemorySize(r);if(dest>total||cap>total-dest||cap<2||cap>8192)return m3Err_trapOutOfBoundsMemoryAccess;
    char path[DM_PATH_MAX];if(guest_string(r,mem,pathoff,DM_PATH_MAX,path,sizeof(path))<0){*ret=-1;return m3Err_none;}DmDirectoryEntry entries[16];int n=dm_fs_list(path,entries,16,(int)offset);if(n<0){*ret=n;return m3Err_none;}char *out=(char*)mem+dest;size_t used=0;
    for(int i=0;i<n;i++){int wrote=snprintf(out+used,cap-used,"%c\t%llu\t%s\n",entries[i].directory?'D':'F',(unsigned long long)entries[i].size,entries[i].name);if(wrote<0||(size_t)wrote>=cap-used)break;used+=(size_t)wrote;}if(used>=cap){*ret=-1;return m3Err_none;}out[used]=0;*ret=(int32_t)used;return m3Err_none;
}
static const void *host_fs_is_directory(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)c;int32_t *ret=(int32_t*)sp++;char path[DM_PATH_MAX];if(guest_string(r,mem,(uint32_t)*sp++,DM_PATH_MAX,path,sizeof(path))<0)*ret=0;else *ret=dm_fs_is_directory(path);return m3Err_none;}
static const void *host_fs_mkdir(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)c;int32_t *ret=(int32_t*)sp++;char path[DM_PATH_MAX];if(guest_string(r,mem,(uint32_t)*sp++,DM_PATH_MAX,path,sizeof(path))<0)*ret=-1;else *ret=dm_fs_mkdir(path);return m3Err_none;}
static const void *host_fs_pair(IM3Runtime r,uint64_t *sp,void *mem,int op){int32_t *ret=(int32_t*)sp++;uint32_t a=(uint32_t)*sp++,b=(uint32_t)*sp++;char pa[DM_PATH_MAX],pb[DM_PATH_MAX];if(guest_string(r,mem,a,DM_PATH_MAX,pa,sizeof(pa))<0||guest_string(r,mem,b,DM_PATH_MAX,pb,sizeof(pb))<0){*ret=-1;return m3Err_none;}*ret=op==0?dm_fs_copy(pa,pb):dm_fs_rename(pa,pb);return m3Err_none;}
static const void *host_fs_copy(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)c;return host_fs_pair(r,sp,mem,0);}
static const void *host_fs_rename(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)c;return host_fs_pair(r,sp,mem,1);}
static const void *host_fs_path_update(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)c;uint32_t path=(uint32_t)sp[0],cap=(uint32_t)sp[1],seen=(uint32_t)sp[2],total=m3_GetMemorySize(r);if(!cap||cap>DM_PATH_MAX||path>total||cap>total-path||seen>total-sizeof(uint64_t)||!memchr((uint8_t*)mem+path,0,cap))return m3Err_trapOutOfBoundsMemoryAccess;dm_fs_path_update((char*)mem+path,cap,(uint64_t*)((uint8_t*)mem+seen));return m3Err_none;}
static const void *host_open_file(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)c;char path[DM_PATH_MAX];if(guest_string(r,mem,(uint32_t)sp[0],DM_PATH_MAX,path,sizeof(path))<0)return m3Err_none;dm_open_file(path);return m3Err_none;}
static const void *host_launch(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)c;int32_t *ret=(int32_t*)sp++;char id[64],arg[DM_PATH_MAX];if(guest_string(r,mem,(uint32_t)*sp++,63,id,sizeof(id))<0||guest_string(r,mem,(uint32_t)*sp++,DM_PATH_MAX,arg,sizeof(arg))<0){*ret=-1;return m3Err_none;}*ret=dm_launch(id,arg)?0:-1;return m3Err_none;}
static void wasm_event_call(DmWindow*w,int type,int result){if(!w||!w->used)return;WasmInstance*s=w->state;if(s->failed||!s->event)return;char a[16],b[16];snprintf(a,sizeof(a),"%d",type);snprintf(b,sizeof(b),"%d",result);const char*args[]={a,b};DmWindow*prior=dm_current_window;dm_current_window=w;M3Result e=wasm_call_argv(s->event,2,args);dm_current_window=prior;if(e)failure(w,"event",e);}
static void wasm_confirm_result(int yes,void*ctx){wasm_event_call(ctx,1,yes);}
static void wasm_operation_result(int result,void*ctx){wasm_event_call(ctx,2,result);}
static const void *host_confirm(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)c;int32_t*ret=(int32_t*)sp++;char title[96],message[256];if(guest_string(r,mem,(uint32_t)*sp++,95,title,sizeof(title))<0||guest_string(r,mem,(uint32_t)*sp++,255,message,sizeof(message))<0){*ret=-1;return m3Err_none;}*ret=dm_confirm(title,message,wasm_confirm_result,m3_GetUserData(r));return m3Err_none;}
static const void *host_copy_async(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)c;int32_t*ret=(int32_t*)sp++;char src[DM_PATH_MAX],dst[DM_PATH_MAX];if(guest_string(r,mem,(uint32_t)*sp++,DM_PATH_MAX,src,sizeof(src))<0||guest_string(r,mem,(uint32_t)*sp++,DM_PATH_MAX,dst,sizeof(dst))<0){*ret=-1;return m3Err_none;}*ret=dm_copy_async(src,dst,wasm_operation_result,m3_GetUserData(r));return m3Err_none;}
static const void *host_trash_async(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)c;int32_t*ret=(int32_t*)sp++;char path[DM_PATH_MAX];if(guest_string(r,mem,(uint32_t)*sp++,DM_PATH_MAX,path,sizeof(path))<0){*ret=-1;return m3Err_none;}*ret=dm_trash_async(path,wasm_operation_result,m3_GetUserData(r));return m3Err_none;}
static size_t wasm_http_receive(char*data,size_t size,size_t count,void*ctx){WasmInstance*s=ctx;size_t n=size*count;if(n>DM_WASM_HTTP_MAX_BODY-s->http_size){s->http_error=1;snprintf(s->http_error_text,sizeof(s->http_error_text),"risposta oltre 8 MiB");return 0;}char*next=realloc(s->http_body,s->http_size+n+1);if(!next){s->http_error=1;snprintf(s->http_error_text,sizeof(s->http_error_text),"memoria insufficiente durante il download");return 0;}s->http_body=next;memcpy(next+s->http_size,data,n);s->http_size+=n;next[s->http_size]=0;return n;}
static void wasm_http_stop(WasmInstance*s){if(s->http_easy){if(s->http_multi)curl_multi_remove_handle(s->http_multi,s->http_easy);curl_easy_cleanup(s->http_easy);s->http_easy=NULL;}if(s->http_multi){curl_multi_cleanup(s->http_multi);s->http_multi=NULL;}free(s->http_body);s->http_body=NULL;s->http_size=0;free(s->http_request_body);s->http_request_body=NULL;s->http_request_size=0;}
static int wasm_http_start(WasmInstance*s,const char*url,const char*body,size_t body_size){wasm_http_stop(s);s->http_done=s->http_error=s->http_delivered=0;s->http_error_text[0]=0;snprintf(s->http_effective_url,sizeof(s->http_effective_url),"%s",url);if(body_size){s->http_request_body=malloc(body_size);if(!s->http_request_body){snprintf(s->http_error_text,sizeof(s->http_error_text),"memoria insufficiente per il modulo");return -1;}memcpy(s->http_request_body,body,body_size);s->http_request_size=body_size;}static int curl_ready;if(!curl_ready){if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK){snprintf(s->http_error_text,sizeof(s->http_error_text),"inizializzazione libcurl fallita");wasm_http_stop(s);return -1;}curl_ready=1;}s->http_multi=curl_multi_init();s->http_easy=curl_easy_init();if(!s->http_multi||!s->http_easy){snprintf(s->http_error_text,sizeof(s->http_error_text),"memoria insufficiente per la richiesta");wasm_http_stop(s);return -1;}
    curl_easy_setopt(s->http_easy,CURLOPT_ERRORBUFFER,s->http_error_text);curl_easy_setopt(s->http_easy,CURLOPT_URL,url);curl_easy_setopt(s->http_easy,CURLOPT_WRITEFUNCTION,wasm_http_receive);curl_easy_setopt(s->http_easy,CURLOPT_WRITEDATA,s);curl_easy_setopt(s->http_easy,CURLOPT_FOLLOWLOCATION,1L);curl_easy_setopt(s->http_easy,CURLOPT_MAXREDIRS,5L);curl_easy_setopt(s->http_easy,CURLOPT_PROTOCOLS_STR,"http,https");curl_easy_setopt(s->http_easy,CURLOPT_REDIR_PROTOCOLS_STR,"http,https");curl_easy_setopt(s->http_easy,CURLOPT_CONNECTTIMEOUT,12L);curl_easy_setopt(s->http_easy,CURLOPT_TIMEOUT,30L);curl_easy_setopt(s->http_easy,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(s->http_easy,CURLOPT_USERAGENT,"DesktopMode/4 (PS Vita; WASM browser)");curl_easy_setopt(s->http_easy,CURLOPT_ACCEPT_ENCODING,"");if(body_size){curl_easy_setopt(s->http_easy,CURLOPT_POST,1L);curl_easy_setopt(s->http_easy,CURLOPT_POSTFIELDS,s->http_request_body);curl_easy_setopt(s->http_easy,CURLOPT_POSTFIELDSIZE_LARGE,(curl_off_t)body_size);}
#ifndef DESKTOP_PREVIEW
    /* app0: is the Vita mount point; desktop preview must use libcurl's system CA store. */
    curl_easy_setopt(s->http_easy,CURLOPT_CAINFO,"app0:/assets/cacert.pem");
#endif
    CURLMcode added=curl_multi_add_handle(s->http_multi,s->http_easy);if(added!=CURLM_OK){snprintf(s->http_error_text,sizeof(s->http_error_text),"libcurl multi: %s",curl_multi_strerror(added));wasm_http_stop(s);return -1;}return 0;}
static const void *host_http_start(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)c;int32_t*ret=(int32_t*)sp++;char url[2048];if(guest_string(r,mem,(uint32_t)*sp++,2047,url,sizeof(url))<0||(strncmp(url,"https://",8)&&strncmp(url,"http://",7))){*ret=-1;return m3Err_none;}*ret=wasm_http_start(((DmWindow*)m3_GetUserData(r))->state,url,NULL,0);return m3Err_none;}
static const void *host_http_post(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)c;int32_t*ret=(int32_t*)sp++;uint32_t url_offset=(uint32_t)*sp++,body_offset=(uint32_t)*sp++,body_size=(uint32_t)*sp++,total=m3_GetMemorySize(r);char url[2048];if(guest_string(r,mem,url_offset,2047,url,sizeof(url))<0||(strncmp(url,"https://",8)&&strncmp(url,"http://",7))||body_size>16384||body_offset>total||body_size>total-body_offset){*ret=-1;return m3Err_none;}*ret=wasm_http_start(((DmWindow*)m3_GetUserData(r))->state,url,(const char*)mem+body_offset,body_size);return m3Err_none;}
static const void *host_http_error(IM3Runtime r,IM3ImportContext c,uint64_t*sp,void*mem){(void)c;int32_t*ret=(int32_t*)sp++;uint32_t dst=(uint32_t)*sp++,cap=(uint32_t)*sp++,total=m3_GetMemorySize(r);if(!cap||cap>512||dst>total||cap>total-dst)return m3Err_trapOutOfBoundsMemoryAccess;WasmInstance*s=((DmWindow*)m3_GetUserData(r))->state;const char*message=s->http_error_text[0]?s->http_error_text:"errore HTTP senza dettagli libcurl";size_t n=strlen(message);if(n>=cap)n=cap-1;memcpy((uint8_t*)mem+dst,message,n);((char*)mem)[dst+n]=0;*ret=(int32_t)n;return m3Err_none;}
static const void *host_http_url(IM3Runtime r,IM3ImportContext c,uint64_t*sp,void*mem){(void)c;int32_t*ret=(int32_t*)sp++;uint32_t dst=(uint32_t)*sp++,cap=(uint32_t)*sp++,total=m3_GetMemorySize(r);if(!cap||cap>2048||dst>total||cap>total-dst)return m3Err_trapOutOfBoundsMemoryAccess;WasmInstance*s=((DmWindow*)m3_GetUserData(r))->state;const char*url=s->http_effective_url;size_t n=strlen(url);if(n>=cap)n=cap-1;memcpy((uint8_t*)mem+dst,url,n);((char*)mem)[dst+n]=0;*ret=(int32_t)n;return m3Err_none;}
static const void *host_js_eval(IM3Runtime r,IM3ImportContext c,uint64_t*sp,void*mem){(void)c;int32_t*ret=(int32_t*)sp++;uint32_t off=(uint32_t)*sp++,n=(uint32_t)*sp++,total=m3_GetMemorySize(r);WasmInstance*s=((DmWindow*)m3_GetUserData(r))->state;if(off>total||n>total-off||n>512u*1024u||!s->js||s->js_failed){*ret=-1;return m3Err_none;}s->js_heap.output[0]=0;s->js_heap.fuel=4096;active_js_heap=&s->js_heap;if(setjmp(browser_js_fatal_jump)){browser_js_fatal_guard=0;s->js_failed=1;active_js_heap=NULL;*ret=-1;return m3Err_none;}browser_js_fatal_guard=1;duk_int_t status=duk_peval_lstring(s->js,(const char*)mem+off,n);browser_js_fatal_guard=0;if(status!=0){duk_safe_to_lstring(s->js,-1,NULL);s->js_failed=0;}duk_pop(s->js);active_js_heap=NULL;*ret=status==0?0:-1;return m3Err_none;}
static const void *host_js_output(IM3Runtime r,IM3ImportContext c,uint64_t*sp,void*mem){(void)c;int32_t*ret=(int32_t*)sp++;uint32_t dst=(uint32_t)*sp++,cap=(uint32_t)*sp++,total=m3_GetMemorySize(r);if(cap<1||cap>BROWSER_JS_OUTPUT_LIMIT||dst>total||cap>total-dst)return m3Err_trapOutOfBoundsMemoryAccess;WasmInstance*s=((DmWindow*)m3_GetUserData(r))->state;size_t n=strlen(s->js_heap.output);if(n>=cap)n=cap-1;memcpy((uint8_t*)mem+dst,s->js_heap.output,n);((char*)mem)[dst+n]=0;*ret=(int32_t)n;return m3Err_none;}
static const void *host_http_poll(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)c;int32_t*ret=(int32_t*)sp++;uint32_t dst=(uint32_t)*sp++,cap=(uint32_t)*sp++,total=m3_GetMemorySize(r);if(dst>total||cap>total-dst||cap>DM_WASM_HTTP_MAX_BODY+1)return m3Err_trapOutOfBoundsMemoryAccess;WasmInstance*s=((DmWindow*)m3_GetUserData(r))->state;if(!s->http_multi){if(!s->http_error_text[0])snprintf(s->http_error_text,sizeof(s->http_error_text),"richiesta HTTP non inizializzata");*ret=-1;return m3Err_none;}if(!s->http_done){int running=0;CURLMcode mc=curl_multi_perform(s->http_multi,&running);if(mc!=CURLM_OK){s->http_error=1;s->http_done=1;snprintf(s->http_error_text,sizeof(s->http_error_text),"libcurl multi: %s",curl_multi_strerror(mc));if(s->http_easy){curl_multi_remove_handle(s->http_multi,s->http_easy);curl_easy_cleanup(s->http_easy);s->http_easy=NULL;}}int left=0;CURLMsg*m;while(!s->http_done&&(m=curl_multi_info_read(s->http_multi,&left)))if(m->msg==CURLMSG_DONE){char*effective=NULL;if(s->http_easy&&curl_easy_getinfo(s->http_easy,CURLINFO_EFFECTIVE_URL,&effective)==CURLE_OK&&effective&&effective[0])snprintf(s->http_effective_url,sizeof(s->http_effective_url),"%s",effective);if(m->data.result!=CURLE_OK){s->http_error=1;if(!s->http_error_text[0])snprintf(s->http_error_text,sizeof(s->http_error_text),"%s",curl_easy_strerror(m->data.result));}s->http_done=1;curl_multi_remove_handle(s->http_multi,s->http_easy);curl_easy_cleanup(s->http_easy);s->http_easy=NULL;}if(!s->http_done){*ret=-2;return m3Err_none;}}
    if(s->http_error||s->http_delivered){*ret=-1;return m3Err_none;}if(s->http_size>=cap){snprintf(s->http_error_text,sizeof(s->http_error_text),"buffer WASM troppo piccolo per la risposta");*ret=-1;return m3Err_none;}if(s->http_size)memcpy((uint8_t*)mem+dst,s->http_body,s->http_size);((char*)mem)[dst+s->http_size]=0;s->http_delivered=1;*ret=(int32_t)s->http_size;return m3Err_none;}
static void wasm_pdf_close(WasmInstance*s){if(s->pdf_engine)pdf_engine_close(s->pdf_engine);s->pdf_engine=NULL;free(s->pdf_data);s->pdf_data=NULL;}
static const void *host_pdf_open(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)c;int32_t*ret=(int32_t*)sp++;uint32_t po=(uint32_t)*sp++,wo=(uint32_t)*sp++,eo=(uint32_t)*sp++,ec=(uint32_t)*sp++,total=m3_GetMemorySize(r);if(eo>total||ec>total-eo||ec>512)return m3Err_trapOutOfBoundsMemoryAccess;char path[DM_PATH_MAX],password[128],error[512]={0};if(guest_string(r,mem,po,DM_PATH_MAX,path,sizeof(path))<0||guest_string(r,mem,wo,127,password,sizeof(password))<0){*ret=-1;return m3Err_none;}WasmInstance*s=((DmWindow*)m3_GetUserData(r))->state;wasm_pdf_close(s);size_t cap=512*1024+1;int n=-1;while(cap<=16*1024*1024+1){s->pdf_data=malloc(cap);if(!s->pdf_data){snprintf(error,sizeof(error),"Memoria insufficiente");break;}n=dm_fs_read(path,(char*)s->pdf_data,cap);if(n!=-2)break;free(s->pdf_data);s->pdf_data=NULL;cap=(cap-1)*2+1;}if(!s->pdf_data||n<0){free(s->pdf_data);s->pdf_data=NULL;if(!error[0])snprintf(error,sizeof(error),"PDF non leggibile o oltre 16 MiB");}else{s->pdf_engine=pdf_engine_open(s->pdf_data,(size_t)n,password[0]?password:NULL,error,sizeof(error));if(!s->pdf_engine){free(s->pdf_data);s->pdf_data=NULL;}else *ret=pdf_engine_pages(s->pdf_engine);}if(!s->pdf_engine)*ret=-1;size_t en=strlen(error);if(en>=ec)en=ec?ec-1:0;if(ec){memcpy((uint8_t*)mem+eo,error,en);((char*)mem)[eo+en]=0;}return m3Err_none;}
static const void *host_pdf_close(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)c;(void)sp;(void)mem;wasm_pdf_close(((DmWindow*)m3_GetUserData(r))->state);return m3Err_none;}
static const void *host_pdf_render(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)c;int32_t*ret=(int32_t*)sp++;int page=(int)*sp++,zoom=(int)*sp++,ox=(int)*sp++,oy=(int)*sp++,w=(int)*sp++,h=(int)*sp++;uint32_t pixels=(uint32_t)*sp++,mx=(uint32_t)*sp++,my=(uint32_t)*sp++,err=(uint32_t)*sp++,ec=(uint32_t)*sp++,total=m3_GetMemorySize(r);uint64_t bytes=(uint64_t)w*h*4;if(w<1||h<1||w>960||h>544||pixels>total||bytes>total-pixels||mx>total-4||my>total-4||err>total||ec>total-err||ec>512)return m3Err_trapOutOfBoundsMemoryAccess;WasmInstance*s=((DmWindow*)m3_GetUserData(r))->state;char error[512]={0};int x=0,y=0;*ret=s->pdf_engine?pdf_engine_render(s->pdf_engine,page,zoom,ox,oy,w,h,(uint32_t*)((uint8_t*)mem+pixels),&x,&y,error,sizeof(error)):-1;memcpy((uint8_t*)mem+mx,&x,4);memcpy((uint8_t*)mem+my,&y,4);size_t n=strlen(error);if(n>=ec)n=ec?ec-1:0;if(ec){memcpy((uint8_t*)mem+err,error,n);((char*)mem)[err+n]=0;}return m3Err_none;}
static const void *host_clipboard_set(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){
    (void)c;char text[DM_TEXT_MAX];if(guest_string(r,mem,(uint32_t)sp[0],DM_TEXT_MAX,text,sizeof(text))<0)return m3Err_trapOutOfBoundsMemoryAccess;dm_clipboard_text_set(text);return m3Err_none;
}
static const void *host_clipboard_get(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){
    (void)c;int32_t *ret=(int32_t*)sp++;uint32_t dst=(uint32_t)*sp++,cap=(uint32_t)*sp++,total=m3_GetMemorySize(r);if(dst>total||cap>total-dst)return m3Err_trapOutOfBoundsMemoryAccess;
    const char *text=dm_clipboard_text_get();size_t n=strlen(text);if(n>=cap)n=cap?cap-1:0;if(cap)((char*)mem)[dst+n]=0;if(n)memcpy((char*)mem+dst,text,n);*ret=(int32_t)n;return m3Err_none;
}
static void wasm_file_chosen(const char *path,int replace,void *context){
    (void)replace;DmWindow *w=context;if(!w||!w->used||!path)return;WasmInstance *s=w->state;
    if(s->failed||!s->runtime||!s->file_result)return;
    uint32_t total=0;uint8_t *memory=m3_GetMemory(s->runtime,&total,0);uint32_t cap=s->pending_file_capacity,offset=s->pending_file_offset;
    size_t n=strlen(path);if(offset>=total||cap>total-offset||n>=cap){dm_status("Percorso troppo lungo per l'app");return;}
    memcpy(memory+offset,path,n+1);char length[16];snprintf(length,sizeof(length),"%u",(unsigned)n);const char *args[]={length};
    DmWindow *prior=dm_current_window;dm_current_window=w;M3Result e=wasm_call_argv(s->file_result,1,args);dm_current_window=prior;if(e)failure(w,"file dialog",e);
}
static const void *host_file_dialog(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){
    (void)c;int32_t *ret=(int32_t*)sp++;uint32_t titleoff=(uint32_t)*sp++,extoff=(uint32_t)*sp++,save=(uint32_t)*sp++,output=(uint32_t)*sp++,cap=(uint32_t)*sp++,total=m3_GetMemorySize(r);
    if(output>total||cap>total-output||cap>DM_PATH_MAX)return m3Err_trapOutOfBoundsMemoryAccess;
    char title[96],ext[128];if(guest_string(r,mem,titleoff,95,title,sizeof(title))<0||guest_string(r,mem,extoff,127,ext,sizeof(ext))<0){*ret=-1;return m3Err_none;}
    DmWindow *w=m3_GetUserData(r);WasmInstance *s=w->state;if(!s->file_result||!cap){*ret=-1;return m3Err_none;}
    s->pending_file_offset=output;s->pending_file_capacity=cap;
    DmFileDialogOptions options={save?DM_FILE_SAVE:DM_FILE_OPEN,title,save?"ux0:/data/desktop-mode/Desktop/":"ux0:/data/","",ext};
    *ret=dm_file_dialog(&options,wasm_file_chosen,w);return m3Err_none;
}
static const void *host_file_dialog_at(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){
    (void)c;int32_t *ret=(int32_t*)sp++;uint32_t titleoff=(uint32_t)*sp++,extoff=(uint32_t)*sp++,save=(uint32_t)*sp++,diroff=(uint32_t)*sp++,output=(uint32_t)*sp++,cap=(uint32_t)*sp++,total=m3_GetMemorySize(r);
    if(output>total||cap>total-output||cap>DM_PATH_MAX)return m3Err_trapOutOfBoundsMemoryAccess;
    char title[96],ext[128],directory[DM_PATH_MAX];if(guest_string(r,mem,titleoff,95,title,sizeof(title))<0||guest_string(r,mem,extoff,127,ext,sizeof(ext))<0||guest_string(r,mem,diroff,DM_PATH_MAX-1,directory,sizeof(directory))<0){*ret=-1;return m3Err_none;}
    DmWindow *w=m3_GetUserData(r);WasmInstance *s=w->state;if(!s->file_result||!cap){*ret=-1;return m3Err_none;}
    s->pending_file_offset=output;s->pending_file_capacity=cap;
    DmFileDialogOptions options={save?DM_FILE_SAVE:DM_FILE_OPEN,title,directory, "",ext};
    *ret=dm_file_dialog(&options,wasm_file_chosen,w);return m3Err_none;
}
static void wasm_prompt_done(const char *text,void *context){
    DmWindow *w=context;if(!w||!w->used)return;WasmInstance *s=w->state;if(s->failed||!s->runtime||!s->text)return;
    uint32_t total=0;uint8_t *memory=m3_GetMemory(s->runtime,&total,0);uint32_t off=s->pending_text_offset,cap=s->pending_text_capacity;size_t n=strlen(text);
    if(off>=total||cap>total-off||n>=cap){dm_status("Testo troppo lungo per l'app");return;}
    memcpy(memory+off,text,n+1);char value[16];snprintf(value,sizeof(value),"%u",(unsigned)n);const char *args[]={value};
    DmWindow *prior=dm_current_window;dm_current_window=w;M3Result e=wasm_call_argv(s->text,1,args);dm_current_window=prior;if(e)failure(w,"text prompt",e);
}
static const void *host_text_prompt(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){
    (void)c;int32_t *ret=(int32_t*)sp++;uint32_t titleoff=(uint32_t)*sp++,initialoff=(uint32_t)*sp++,output=(uint32_t)*sp++,cap=(uint32_t)*sp++,total=m3_GetMemorySize(r);
    if(output>total||cap>total-output||cap>DM_TEXT_MAX)return m3Err_trapOutOfBoundsMemoryAccess;
    char title[96],initial[DM_TEXT_MAX];if(guest_string(r,mem,titleoff,95,title,sizeof(title))<0||guest_string(r,mem,initialoff,DM_TEXT_MAX,initial,sizeof(initial))<0){*ret=-1;return m3Err_none;}
    DmWindow *w=m3_GetUserData(r);WasmInstance *s=w->state;s->pending_text_offset=output;s->pending_text_capacity=cap;
    dm_prompt(title,initial,wasm_prompt_done,w);*ret=0;return m3Err_none;
}
static const void *host_network_info(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){
    (void)c;int32_t*ret=(int32_t*)sp++;uint32_t out=(uint32_t)*sp++,cap=(uint32_t)*sp++,total=m3_GetMemorySize(r);
    if(out>total||cap>total-out||cap<sizeof(DmNetworkInfo))return m3Err_trapOutOfBoundsMemoryAccess;
    *ret=dm_network_service_info(m3_GetUserData(r),(DmNetworkInfo*)((uint8_t*)mem+out));return m3Err_none;
}
static const void *host_network_row(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){
    (void)c;int32_t*ret=(int32_t*)sp++;int index=(int32_t)*sp++;uint32_t out=(uint32_t)*sp++,cap=(uint32_t)*sp++,total=m3_GetMemorySize(r);
    if(out>total||cap>total-out||cap<sizeof(DmNetworkRow))return m3Err_trapOutOfBoundsMemoryAccess;
    *ret=dm_network_service_row(m3_GetUserData(r),index,(DmNetworkRow*)((uint8_t*)mem+out));return m3Err_none;
}
static const void *host_network_action(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){
    (void)c;(void)mem;int32_t*ret=(int32_t*)sp++;int action=(int32_t)*sp++;*ret=dm_network_service_action(m3_GetUserData(r),action);return m3Err_none;
}
static const void *host_network_select(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){
    (void)c;(void)mem;int32_t*ret=(int32_t*)sp++;int index=(int32_t)*sp++,activate=(int32_t)*sp++;*ret=dm_network_service_select(m3_GetUserData(r),index,activate);return m3Err_none;
}
static const void *host_media_open(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)c;int32_t*ret=(int32_t*)sp++;char path[DM_PATH_MAX];if(guest_string(r,mem,(uint32_t)*sp++,DM_PATH_MAX,path,sizeof(path))<0)*ret=-1;else *ret=dm_media_player_open(path);return m3Err_none;}
static const void *host_media_action(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)r;(void)c;(void)mem;int32_t*ret=(int32_t*)sp++;*ret=dm_media_player_action((int32_t)*sp++);return m3Err_none;}
static const void *host_media_status(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)r;(void)c;(void)mem;int32_t*ret=(int32_t*)sp++;*ret=dm_media_player_status();return m3Err_none;}
static const void *host_media_time_ms(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)r;(void)c;(void)mem;int32_t*ret=(int32_t*)sp++;dm_media_player_tick();*ret=dm_media_player_time_ms();return m3Err_none;}
static const void *host_media_duration_ms(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)r;(void)c;(void)mem;int32_t*ret=(int32_t*)sp++;*ret=dm_media_player_duration_ms();return m3Err_none;}
static const void *host_media_seek_ms(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)r;(void)c;(void)mem;int32_t*ret=(int32_t*)sp++;*ret=dm_media_player_seek_ms((int32_t)*sp++);return m3Err_none;}
static const void *host_media_set_volume(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)r;(void)c;(void)mem;int32_t*ret=(int32_t*)sp++;*ret=dm_media_player_set_volume((int32_t)*sp++);return m3Err_none;}
static const void *host_media_get_volume(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)r;(void)c;(void)mem;int32_t*ret=(int32_t*)sp++;*ret=dm_media_player_get_volume();return m3Err_none;}
static const void *host_media_metadata(IM3Runtime r,IM3ImportContext c,uint64_t *sp,void *mem){(void)c;int32_t*ret=(int32_t*)sp++;uint32_t artist=(uint32_t)*sp++,ac=(uint32_t)*sp++,title=(uint32_t)*sp++,tc=(uint32_t)*sp++,total=m3_GetMemorySize(r);if(artist>total||ac>total-artist||title>total||tc>total-title||ac>256||tc>256)return m3Err_trapOutOfBoundsMemoryAccess;dm_media_player_metadata((char*)mem+artist,ac,(char*)mem+title,tc);*ret=0;return m3Err_none;}
static M3Result link_optional(IM3Module module,const char *name,const char *sig,M3RawCall fn){
    M3Result e=m3_LinkRawFunction(module,"desktop",name,sig,fn);
    return e==m3Err_functionLookupFailed?m3Err_none:e;
}
static M3Result link_host_imports(IM3Module module){
    M3Result e=m3_LinkRawFunction(module,"desktop","rect","v(iiiii)",host_rect);
    if(!e)e=m3_LinkRawFunction(module,"desktop","text","v(iiii)",host_text);
    if(!e)e=link_optional(module,"text_scaled","v(iiiii)",host_text_scaled);
    if(!e)e=link_optional(module,"text_width","i(i)",host_text_width);
    if(!e)e=link_optional(module,"text_width_scaled","i(ii)",host_text_width_scaled);
    if(!e)e=link_optional(module,"scrollbar_draw","v(iiiiiiii)",host_scrollbar_draw);
    if(!e)e=link_optional(module,"task_count","i()",host_task_count);
    if(!e)e=link_optional(module,"task_get","i(iii)",host_task_get);
    if(!e)e=link_optional(module,"task_memory","i(i)",host_task_memory);
    if(!e)e=link_optional(module,"task_action","i(ii)",host_task_action);
    if(!e)e=link_optional(module,"image_load","i(i)",host_image_load);
    if(!e)e=link_optional(module,"image_size","i(iii)",host_image_size);
    if(!e)e=link_optional(module,"image_read","i(iii)",host_image_read);
    if(!e)e=link_optional(module,"image_draw","v(iiiii)",host_image_draw);
    if(!e)e=link_optional(module,"image_draw_clipped","v(iiiiiiiii)",host_image_draw_clipped);
    if(!e)e=link_optional(module,"image_free","v(i)",host_image_free);
    if(!e)e=link_optional(module,"image_create","i(iii)",host_image_create);
    if(!e)e=link_optional(module,"image_update","v(ii)",host_image_update);
    if(!e)e=link_optional(module,"image_save_png","i(iiiii)",host_image_save_png);
    if(!e)e=link_optional(module,"pointer_state","i(iii)",host_pointer_state);
    if(!e)e=link_optional(module,"fs_read","i(iii)",host_fs_read);
    if(!e)e=link_optional(module,"fs_write","i(iiii)",host_fs_write);
    if(!e)e=link_optional(module,"fs_list_text","i(iiii)",host_fs_list_text);
    if(!e)e=link_optional(module,"fs_is_directory","i(i)",host_fs_is_directory);
    if(!e)e=link_optional(module,"fs_mkdir","i(i)",host_fs_mkdir);
    if(!e)e=link_optional(module,"fs_copy","i(ii)",host_fs_copy);
    if(!e)e=link_optional(module,"fs_rename","i(ii)",host_fs_rename);
    if(!e)e=link_optional(module,"fs_path_update","v(iii)",host_fs_path_update);
    if(!e)e=link_optional(module,"open_file","v(i)",host_open_file);
    if(!e)e=link_optional(module,"launch","i(ii)",host_launch);
    if(!e)e=link_optional(module,"confirm","i(ii)",host_confirm);
    if(!e)e=link_optional(module,"copy_async","i(ii)",host_copy_async);
    if(!e)e=link_optional(module,"trash_async","i(i)",host_trash_async);
    if(!e)e=link_optional(module,"http_start","i(i)",host_http_start);
    if(!e)e=link_optional(module,"http_post","i(iii)",host_http_post);
    if(!e)e=link_optional(module,"http_poll","i(ii)",host_http_poll);
    if(!e)e=link_optional(module,"http_error","i(ii)",host_http_error);
    if(!e)e=link_optional(module,"http_url","i(ii)",host_http_url);
    if(!e)e=link_optional(module,"js_eval","i(ii)",host_js_eval);
    if(!e)e=link_optional(module,"js_output","i(ii)",host_js_output);
    if(!e)e=link_optional(module,"pdf_open","i(iiii)",host_pdf_open);
    if(!e)e=link_optional(module,"pdf_close","v()",host_pdf_close);
    if(!e)e=link_optional(module,"pdf_render","i(iiiiiiiiiii)",host_pdf_render);
    if(!e)e=link_optional(module,"clipboard_set","v(i)",host_clipboard_set);
    if(!e)e=link_optional(module,"clipboard_get","i(ii)",host_clipboard_get);
    if(!e)e=link_optional(module,"file_dialog","i(iiiii)",host_file_dialog);
    if(!e)e=link_optional(module,"file_dialog_at","i(iiiiii)",host_file_dialog_at);
    if(!e)e=link_optional(module,"text_prompt","i(iiii)",host_text_prompt);
    if(!e)e=link_optional(module,"network_info","i(ii)",host_network_info);
    if(!e)e=link_optional(module,"network_row","i(iii)",host_network_row);
    if(!e)e=link_optional(module,"network_action","i(i)",host_network_action);
    if(!e)e=link_optional(module,"network_select","i(ii)",host_network_select);
    if(!e)e=link_optional(module,"media_open","i(i)",host_media_open);
    if(!e)e=link_optional(module,"media_action","i(i)",host_media_action);
    if(!e)e=link_optional(module,"media_status","i()",host_media_status);
    if(!e)e=link_optional(module,"media_time_ms","i()",host_media_time_ms);
    if(!e)e=link_optional(module,"media_duration_ms","i()",host_media_duration_ms);
    if(!e)e=link_optional(module,"media_seek_ms","i(i)",host_media_seek_ms);
    if(!e)e=link_optional(module,"media_set_volume","i(i)",host_media_set_volume);
    if(!e)e=link_optional(module,"media_get_volume","i()",host_media_get_volume);
    if(!e)e=link_optional(module,"media_metadata","i(iiii)",host_media_metadata);
    if(!e){e=m3_LinkRawFunction(module,"env","emscripten_memcpy_big","i(iii)",host_emscripten_memcpy_big);if(e==m3Err_functionLookupFailed)e=m3Err_none;}
    if(!e){e=m3_LinkRawFunction(module,"env","emscripten_resize_heap","i(i)",host_emscripten_resize_heap);if(e==m3Err_functionLookupFailed)e=m3Err_none;}
    return e;
}
static void wasm_open(DmWindow *w, const char *argument) {
    (void)argument;
    WasmApp *app=app_for(w); WasmInstance *s=w->state;
    if(!app){s->failed=1;return;}s->menu_open=-1;
    if(!strcmp(app->id,"browser")){s->js=browser_js_create(&s->js_heap);if(!s->js){failure(w,"Javascript",m3Err_mallocFailed);return;}}
    s->env=m3_NewEnvironment();
    if(!s->env){failure(w,"init",m3Err_mallocFailed);return;}
    s->runtime=m3_NewRuntime(s->env,64*1024,w);
    if(!s->runtime){failure(w,"init",m3Err_mallocFailed);return;}
    s->runtime->memoryLimit=!strcmp(app->id,"browser")?64u*1024u*1024u:!strcmp(app->id,"paint")?20u*1024u*1024u:WASM_MEMORY_LIMIT;
    IM3Module module=NULL;
    M3Result error=m3_ParseModule(s->env,&module,app->bytes,(uint32_t)app->size);
    int loaded=0;
    if(!error){error=m3_LoadModule(s->runtime,module);if(!error)loaded=1;}
    if(error&&module&&!loaded)m3_FreeModule(module);
    if(!error) error=link_host_imports(module);
    if(!error) error=m3_FindFunction(&s->init,s->runtime,"dm_app_init");
    if(!error) error=m3_FindFunction(&s->abi,s->runtime,"dm_app_abi_version");
    if(!error) error=m3_FindFunction(&s->draw,s->runtime,"dm_app_draw");
    if(!error) error=m3_FindFunction(&s->click,s->runtime,"dm_app_click");
    if(!error) error=m3_FindFunction(&s->close,s->runtime,"dm_app_close");
    if(error){failure(w,"load",error);return;}
    if(m3_FindFunction(&s->text_buffer,s->runtime,"dm_app_text_buffer"))s->text_buffer=NULL;
    if(m3_FindFunction(&s->file_buffer,s->runtime,"dm_app_file_buffer"))s->file_buffer=NULL;
    if(m3_FindFunction(&s->file_result,s->runtime,"dm_app_file_result"))s->file_result=NULL;
    if(m3_FindFunction(&s->text,s->runtime,"dm_app_text"))s->text=NULL;
    if(m3_FindFunction(&s->key,s->runtime,"dm_app_key"))s->key=NULL;
    if(m3_FindFunction(&s->tick,s->runtime,"dm_app_tick"))s->tick=NULL;
    if(m3_FindFunction(&s->event,s->runtime,"dm_app_event"))s->event=NULL;
    if(m3_FindFunction(&s->menu,s->runtime,"dm_app_menu"))s->menu=NULL;
    if(m3_FindFunction(&s->dirty,s->runtime,"dm_app_dirty"))s->dirty=NULL;
    if(m3_FindFunction(&s->discard,s->runtime,"dm_app_discard"))s->discard=NULL;
    if(m3_FindFunction(&s->argument_buffer,s->runtime,"dm_app_argument_buffer"))s->argument_buffer=NULL;
    uint32_t argument_offset=0;
    if(s->argument_buffer){int32_t offset=0;error=wasm_call_v(s->argument_buffer);if(!error)error=m3_GetResultsV(s->argument_buffer,&offset);if(error){failure(w,"argument buffer",error);return;}argument_offset=(uint32_t)offset;}
    uint32_t mem_size=0;uint8_t *memory=m3_GetMemory(s->runtime,&mem_size,0);
    if(argument_offset>=mem_size){failure(w,"argument",m3Err_trapOutOfBoundsMemoryAccess);return;}
    const char *launch_arg=argument?argument:"";size_t arg_len=strlen(launch_arg);
    if(arg_len>1023||arg_len>=mem_size-argument_offset){failure(w,"argument",m3Err_trapOutOfBoundsMemoryAccess);return;}
    memcpy(memory+argument_offset,launch_arg,arg_len+1);
    int32_t version=0;
    error=wasm_call_v(s->abi);
    if(!error) error=m3_GetResultsV(s->abi,&version);
    if(!error && version!=1) error="WASM ABI non supportata";
    if(error){failure(w,"ABI",error);return;}
    char arg_ptr[16];snprintf(arg_ptr,sizeof(arg_ptr),"%u",argument_offset);const char *args[]={arg_ptr};
    error=wasm_call_argv(s->init,1,args);
    if(error)failure(w,"init",error);
    else if(!strcmp(app->id,"network")&&dm_network_service_open(w)<0)failure(w,"network service", "servizio rete non disponibile");
}
typedef struct {const char *label;int command;} WasmMenuItem;
static int menu_category_count(const char *id){return !strcmp(id,"notepad")?5:(!strcmp(id,"solitaire")||!strcmp(id,"minesweeper")?2:4);}
static const char *menu_category(const char *id,int n){
    if(!strcmp(id,"solitaire")||!strcmp(id,"minesweeper"))return n==0?dm_localize("Partita","Game","Partida"):dm_localize("Aiuto","Help","Ayuda");
    if(!strcmp(id,"media")){static const char*it[]={"File","Riproduzione","Visualizza","?"};static const char*en[]={"File","Play","View","?"};static const char*es[]={"Archivo","Reproduccion","Ver","?"};return dm_localize(it[n],en[n],es[n]);}
    if(!strcmp(id,"notepad")){static const char*it[]={"File","Modifica","Formato","Visualizza","?"};static const char*en[]={"File","Edit","Format","View","?"};static const char*es[]={"Archivo","Editar","Formato","Ver","?"};return dm_localize(it[n],en[n],es[n]);}
    static const char*it[]={"File","Modifica","Visualizza","Aiuto"};static const char*en[]={"File","Edit","View","Help"};static const char*es[]={"Archivo","Editar","Ver","Ayuda"};return dm_localize(it[n],en[n],es[n]);
}
static int menu_items_for(const char*id,int category,const WasmMenuItem**out){
    static const WasmMenuItem note_file[]={{"Nuovo",DM_WASM_MENU_NEW},{"Apri...",DM_WASM_MENU_OPEN},{"Salva",DM_WASM_MENU_SAVE},{"Salva con nome...",DM_WASM_MENU_SAVE_AS},{"Esci",DM_WASM_MENU_EXIT}};
    static const WasmMenuItem note_edit[]={{"Annulla",DM_WASM_MENU_UNDO},{"Taglia",DM_WASM_MENU_CUT},{"Copia",DM_WASM_MENU_COPY},{"Incolla",DM_WASM_MENU_PASTE},{"Seleziona tutto",DM_WASM_MENU_SELECT_ALL},{"Inserisci testo...",DM_WASM_MENU_INSERT_TEXT}};
    static const WasmMenuItem note_format[]={{"A capo automatico",DM_WASM_MENU_WRAP}};
    static const WasmMenuItem note_view[]={{"Barra di stato",DM_WASM_MENU_STATUS}};
    static const WasmMenuItem note_help[]={{"Informazioni su Blocco note",DM_WASM_MENU_ABOUT}};
    static const WasmMenuItem game[]={{"Nuova partita",DM_WASM_MENU_NEW},{"Esci",DM_WASM_MENU_EXIT}};
    static const WasmMenuItem game_help[]={{"Regole e informazioni",DM_WASM_MENU_ABOUT}};
    static const WasmMenuItem paint_file[]={{"Nuovo",DM_WASM_MENU_NEW},{"Apri...",DM_WASM_MENU_OPEN},{"Salva",DM_WASM_MENU_SAVE},{"Salva con nome...",DM_WASM_MENU_SAVE_AS},{"Esci",DM_WASM_MENU_EXIT}};
    static const WasmMenuItem paint_edit[]={{"Annulla",DM_WASM_MENU_UNDO},{"Taglia",DM_WASM_MENU_CUT},{"Copia",DM_WASM_MENU_COPY},{"Incolla",DM_WASM_MENU_PASTE},{"Seleziona tutto",DM_WASM_MENU_SELECT_ALL}};
    static const WasmMenuItem paint_view[]={{"Zoom avanti",DM_WASM_MENU_ZOOM_IN},{"Zoom indietro",DM_WASM_MENU_ZOOM_OUT},{"Attributi immagine...",DM_WASM_MENU_IMAGE_SIZE}};
    static const WasmMenuItem pdf_file[]={{"Apri...",DM_WASM_MENU_OPEN},{"Esci",DM_WASM_MENU_EXIT}};
    static const WasmMenuItem pdf_view[]={{"Pagina precedente",DM_WASM_MENU_PREVIOUS},{"Pagina successiva",DM_WASM_MENU_NEXT},{"Zoom avanti",DM_WASM_MENU_ZOOM_IN},{"Zoom indietro",DM_WASM_MENU_ZOOM_OUT}};
    static const WasmMenuItem browser_file[]={{"Apri indirizzo...",DM_WASM_MENU_OPEN},{"Ricarica",DM_WASM_MENU_RELOAD},{"Esci",DM_WASM_MENU_EXIT}};
    static const WasmMenuItem browser_edit[]={{"Copia indirizzo",DM_WASM_MENU_COPY}};
    static const WasmMenuItem generic_file[]={{"Chiudi",DM_WASM_MENU_EXIT}};
    static const WasmMenuItem generic_help[]={{"Informazioni...",DM_WASM_MENU_ABOUT}};
    static const WasmMenuItem counter_file[]={{"Azzera contatore",DM_WASM_MENU_RESET},{"Esci",DM_WASM_MENU_EXIT}};
    static const WasmMenuItem calc_edit[]={{"Copia risultato",DM_WASM_MENU_COPY}};
    static const WasmMenuItem image_file[]={{"Apri immagine...",DM_WASM_MENU_OPEN},{"Esci",DM_WASM_MENU_EXIT}};
    static const WasmMenuItem media_file[]={{"Apri file...",DM_WASM_MENU_OPEN},{"Esci",DM_WASM_MENU_EXIT}};
    static const WasmMenuItem media_play[]={{"Riproduci / Pausa",DM_WASM_MENU_MEDIA_TOGGLE_PLAY},{"Stop",DM_WASM_MENU_MEDIA_STOP},{"Indietro di 10 secondi",DM_WASM_MENU_MEDIA_BACK},{"Avanti di 10 secondi",DM_WASM_MENU_MEDIA_FORWARD},{"Traccia precedente",DM_WASM_MENU_PREVIOUS},{"Traccia successiva",DM_WASM_MENU_NEXT}};
    static const WasmMenuItem media_view[]={{"Mostra playlist",DM_WASM_MENU_MEDIA_PLAYLIST},{"Visualizzazione frattale",DM_WASM_MENU_MEDIA_VISUALIZATION}};
    static const WasmMenuItem console_file[]={{"Pulisci schermo",DM_WASM_MENU_CLEAR},{"Esci",DM_WASM_MENU_EXIT}};
    static const WasmMenuItem console_edit[]={{"Copia",DM_WASM_MENU_COPY},{"Incolla",DM_WASM_MENU_PASTE}};
    static const WasmMenuItem refresh_view[]={{"Aggiorna",DM_WASM_MENU_RELOAD}};
    if(!strcmp(id,"notepad")){static const WasmMenuItem*sets[]={note_file,note_edit,note_format,note_view,note_help};static const int counts[]={5,6,1,1,1};if(category>=0&&category<5){*out=sets[category];return counts[category];}}
    if(!strcmp(id,"solitaire")||!strcmp(id,"minesweeper")){if(category==0){*out=game;return 2;}if(category==1){*out=game_help;return 1;}return 0;}
    if(!strcmp(id,"media")){if(category==0){*out=media_file;return 2;}if(category==1){*out=media_play;return 6;}if(category==2){*out=media_view;return 2;}if(category==3){*out=generic_help;return 1;}return 0;}
    if(category==0){if(!strcmp(id,"paint")){*out=paint_file;return 5;}if(!strcmp(id,"pdf")){*out=pdf_file;return 2;}if(!strcmp(id,"browser")){*out=browser_file;return 3;}if(!strcmp(id,"counter")){*out=counter_file;return 2;}if(!strcmp(id,"images")){*out=image_file;return 2;}if(!strcmp(id,"console")){*out=console_file;return 2;}*out=generic_file;return 1;}
    if(category==1){if(!strcmp(id,"paint")){*out=paint_edit;return 5;}if(!strcmp(id,"console")){*out=console_edit;return 2;}if(!strcmp(id,"calculator")){*out=calc_edit;return 1;}if(!strcmp(id,"browser")){*out=browser_edit;return 1;}return 0;}
    if(category==2){if(!strcmp(id,"paint")){*out=paint_view;return 3;}if(!strcmp(id,"pdf")){*out=pdf_view;return 4;}if(!strcmp(id,"browser")||!strcmp(id,"network")||!strcmp(id,"taskmanager")){*out=refresh_view;return 1;}return 0;}
    if(category==3){*out=generic_help;return 1;}return 0;
}
static int menu_scale(void){return 100;}
static int menu_category_width(const char*id,int category){return dm_text_width(menu_category(id,category))+((menu_scale()*12+50)/100);}
static int menu_category_x(const char*id,int category){int x=(menu_scale()*8+50)/100;for(int i=0;i<category;i++)x+=menu_category_width(id,i);return x;}
static const char*menu_item_label(int command,const char*fallback);
static int menu_popup_width(const WasmMenuItem*items,int count){int width=0;for(int i=0;i<count;i++){int item=dm_text_width(menu_item_label(items[i].command,items[i].label));if(item>width)width=item;}return width+(menu_scale()*28+50)/100;}
static int menu_row_height(void){int h=(menu_scale()*25+50)/100;return h<22?22:h;}
static const char*menu_item_label(int command,const char*fallback){switch(command){
    case DM_WASM_MENU_NEW:return dm_localize("Nuovo","New","Nuevo");case DM_WASM_MENU_OPEN:return dm_localize("Apri...","Open...","Abrir...");case DM_WASM_MENU_SAVE:return dm_localize("Salva","Save","Guardar");case DM_WASM_MENU_SAVE_AS:return dm_localize("Salva con nome...","Save as...","Guardar como...");case DM_WASM_MENU_EXIT:return dm_localize("Esci","Exit","Salir");
    case DM_WASM_MENU_CUT:return dm_localize("Taglia","Cut","Cortar");case DM_WASM_MENU_COPY:return dm_localize("Copia","Copy","Copiar");case DM_WASM_MENU_PASTE:return dm_localize("Incolla","Paste","Pegar");case DM_WASM_MENU_SELECT_ALL:return dm_localize("Seleziona tutto","Select all","Seleccionar todo");case DM_WASM_MENU_UNDO:return dm_localize("Annulla","Undo","Deshacer");
    case DM_WASM_MENU_ZOOM_IN:return dm_localize("Zoom avanti","Zoom in","Acercar");case DM_WASM_MENU_ZOOM_OUT:return dm_localize("Zoom indietro","Zoom out","Alejar");case DM_WASM_MENU_ABOUT:return dm_localize("Informazioni...","About...","Acerca de...");case DM_WASM_MENU_RELOAD:return dm_localize("Aggiorna","Refresh","Actualizar");
    case DM_WASM_MENU_PREVIOUS:return dm_localize("Pagina precedente","Previous page","Página anterior");case DM_WASM_MENU_NEXT:return dm_localize("Pagina successiva","Next page","Página siguiente");case DM_WASM_MENU_RESET:return dm_localize("Azzera contatore","Reset counter","Reiniciar contador");case DM_WASM_MENU_WRAP:return dm_localize("A capo automatico","Word wrap","Ajuste de línea");case DM_WASM_MENU_STATUS:return dm_localize("Barra di stato","Status bar","Barra de estado");case DM_WASM_MENU_CLEAR:return dm_localize("Pulisci schermo","Clear screen","Limpiar pantalla");case DM_WASM_MENU_INSERT_TEXT:return dm_localize("Inserisci testo...","Insert text...","Insertar texto...");default:return fallback;}}
static void wasm_menu_draw(DmWindow*w,WasmInstance*s){
    WasmApp*a=app_for(w);if(!a)return;int cats=menu_category_count(a->id);dm_rect(w->x,w->y+40,w->w,24,DM_COLOR(241,244,248,255));
    for(int i=0;i<cats;i++){int x=menu_category_x(a->id,i),cw=menu_category_width(a->id,i);if(s->menu_open==i)dm_rect(w->x+x-3,w->y+41,cw+6,22,DM_COLOR(205,222,240,255));dm_text(w->x+x,w->y+57,menu_category(a->id,i),DM_COLOR(25,37,51,255));}
    if(s->menu_open<0||s->menu_open>=cats)return;
    const WasmMenuItem*items=NULL;int n=menu_items_for(a->id,s->menu_open,&items);if(!n)return;
    int x=menu_category_x(a->id,s->menu_open)-4,y=64,rh=menu_row_height(),mw=menu_popup_width(items,n),height=n*rh;if(y+height>w->h-2)height=w->h-y-2;dm_rect(w->x+x,w->y+y,mw,height,DM_COLOR(250,251,253,255));dm_rect(w->x+x,w->y+y,mw,1,DM_COLOR(112,132,153,255));
    for(int i=0;i<n&&y+i*rh+rh-1<w->h;i++){dm_text(w->x+x+(menu_scale()*10+50)/100,w->y+y+(menu_scale()*17+50)/100+i*rh,menu_item_label(items[i].command,items[i].label),DM_COLOR(25,37,51,255));dm_rect(w->x+x,w->y+y+(i+1)*rh-1,mw,1,DM_COLOR(230,234,239,255));}
}
static void wasm_menu_command(DmWindow*w,int command){
    WasmInstance*s=w->state;if(command==DM_WASM_MENU_EXIT){dm_close(w);return;}
    if(command==DM_WASM_MENU_ABOUT){WasmApp*a=app_for(w);dm_status(a?a->title:"Desktop Mode");return;}
    int key=0;if(command==DM_WASM_MENU_SAVE)key=DM_WASM_KEY_SAVE;else if(command==DM_WASM_MENU_CUT)key=DM_WASM_KEY_CUT;else if(command==DM_WASM_MENU_COPY)key=DM_WASM_KEY_COPY;else if(command==DM_WASM_MENU_PASTE)key=DM_WASM_KEY_PASTE;else if(command==DM_WASM_MENU_SELECT_ALL)key=DM_WASM_KEY_SELECT_ALL;
    if(key&&(!s->menu||command!=DM_WASM_MENU_COPY)){wasm_key(w,key);return;}
    if(s->menu){char value[16];snprintf(value,sizeof(value),"%d",command);const char*args[]={value};M3Result e=wasm_call_argv(s->menu,1,args);if(e)failure(w,"menu",e);return;}
    if(key)wasm_key(w,key);else dm_status("Comando non disponibile per questa app");
}
static void wasm_draw(DmWindow *w) {
    WasmInstance *s=w->state;
    if(s->failed){dm_text(w->x+20,w->y+80,"Applicazione terminata per errore.",DM_COLOR(130,30,30,255));if(s->failure_message[0])dm_text(w->x+20,w->y+104,s->failure_message,DM_COLOR(90,35,35,255));return;}
    char width[16],height[16]; snprintf(width,sizeof(width),"%d",wasm_client_width(w)); snprintf(height,sizeof(height),"%d",wasm_client_height(w));
    const char *args[]={width,height}; M3Result e=wasm_call_argv(s->draw,2,args);
    if(e)failure(w,"draw",e);
    else wasm_menu_draw(w,s);
}
static void wasm_click(DmWindow *w,int x,int y) {
    WasmInstance *s=w->state; if(s->failed)return;WasmApp*a=app_for(w);int cats=a?menu_category_count(a->id):0;
    if(y>=40&&y<64){int hit=-1;for(int i=0;i<cats;i++){int left=menu_category_x(a->id,i)-4;if(x>=left&&x<left+menu_category_width(a->id,i)+8)hit=i;}if(hit>=0){s->menu_open=s->menu_open==hit?-1:hit;return;}}
    if(s->menu_open>=0){if(y>=64){const WasmMenuItem*items=NULL;int n=menu_items_for(a->id,s->menu_open,&items);int left=menu_category_x(a->id,s->menu_open)-4,rh=menu_row_height(),mw=menu_popup_width(items,n);if(x>=left&&x<left+mw&&y<64+n*rh){int index=(y-64)/rh;if(index<n){int command=items[index].command;s->menu_open=-1;wasm_menu_command(w,command);return;}}}s->menu_open=-1;return;}
    if(y<64)return;
    char sx[16],sy[16],buttons[8]; snprintf(sx,sizeof(sx),"%d",x); snprintf(sy,sizeof(sy),"%d",y-64); snprintf(buttons,sizeof(buttons),"1");
    const char *args[]={sx,sy,buttons}; M3Result e=wasm_call_argv(s->click,3,args);
    if(e)failure(w,"click",e);
}
static void wasm_text(DmWindow *w,const char *text){
    WasmInstance *s=w->state;if(s->failed||!s->text||!s->text_buffer)return;
    int32_t offset=0;M3Result e=wasm_call_v(s->text_buffer);if(!e)e=m3_GetResultsV(s->text_buffer,&offset);
    uint32_t size=0;uint8_t *memory=m3_GetMemory(s->runtime,&size,0);size_t n=strlen(text);
    if(e||offset<0||(uint32_t)offset>=size||n>=size-(uint32_t)offset||n>1024){failure(w,"text buffer",e?e:m3Err_trapOutOfBoundsMemoryAccess);return;}
    memcpy(memory+offset,text,n);memory[offset+n]=0;char length[16];snprintf(length,sizeof(length),"%u",(unsigned)n);const char *args[]={length};e=wasm_call_argv(s->text,1,args);if(e)failure(w,"text",e);
}
static void wasm_key(DmWindow *w,int key){WasmInstance *s=w->state;if(s->failed||!s->key)return;char value[16];snprintf(value,sizeof(value),"%d",key);const char *args[]={value};M3Result e=wasm_call_argv(s->key,1,args);if(e)failure(w,"key",e);}
static void wasm_tick(DmWindow *w,unsigned elapsed){WasmInstance *s=w->state;if(s->failed)return;WasmApp*app=app_for(w);if(app&&!strcmp(app->id,"network"))dm_network_service_tick(w,elapsed);if(!s->tick)return;char value[16];snprintf(value,sizeof(value),"%u",elapsed);const char *args[]={value};M3Result e=wasm_call_argv(s->tick,1,args);if(e)failure(w,"tick",e);}
static void wasm_close_confirmed(int yes,void*ctx){DmWindow*w=ctx;if(!w||!w->used)return;WasmInstance*s=w->state;s->close_prompted=0;if(!yes)return;if(s->discard&&!s->failed){M3Result e=wasm_call_v(s->discard);if(e){failure(w,"discard",e);return;}}dm_close(w);}
static int wasm_close(DmWindow *w) {
    WasmInstance *s=w->state;
    if(!s->failed&&s->dirty&&!s->close_prompted){int32_t dirty=0;M3Result e=wasm_call_v(s->dirty);if(!e)e=m3_GetResultsV(s->dirty,&dirty);if(e){failure(w,"dirty state",e);return 0;}if(dirty){WasmApp*app=app_for(w);const char*title=app&&!strcmp(app->id,"paint")?"Paint":"Blocco note";const char*message=app&&!strcmp(app->id,"paint")?"Il disegno non salvato andrà perso. Chiudere?":"Le modifiche non salvate andranno perse. Chiudere?";s->close_prompted=1;if(dm_confirm(title,message,wasm_close_confirmed,w)<0){s->close_prompted=0;dm_status("Impossibile chiedere conferma per la chiusura");}return 0;}}
    WasmApp*app=app_for(w);if(app&&!strcmp(app->id,"network"))dm_network_service_close(w);if(app&&!strcmp(app->id,"media"))dm_media_player_close();
    if(s->close&&!s->failed){M3Result e=wasm_call_v(s->close);if(e)failure(w,"close",e);}
    if(s->runtime)m3_FreeRuntime(s->runtime);
    if(s->js&&!s->js_failed){active_js_heap=&s->js_heap;duk_destroy_heap(s->js);active_js_heap=NULL;}browser_js_reclaim(&s->js_heap);
    for(int i=0;i<8;i++)if(s->images[i])dm_image_free(s->images[i]);
    wasm_http_stop(s);
    wasm_pdf_close(s);
    if(s->env)m3_FreeEnvironment(s->env);
    memset(s,0,sizeof(*s));
    return 1;
}
int dm_wasm_register_package(const char *id,const char *title,const uint8_t *bytes,size_t size) {
    if(!id||!id[0]||strlen(id)>=64||!title||strlen(title)>=96||!bytes||size<8||size>WASM_MODULE_LIMIT||app_count>=WASM_APP_LIMIT)return -1;
    for(unsigned i=0;i<app_count;i++)if(!strcmp(apps[i].id,id))return -1;
    WasmApp *a=&apps[app_count]; memset(a,0,sizeof(*a));
    a->bytes=malloc(size); if(!a->bytes)return -1;
    memcpy(a->bytes,bytes,size);a->size=size;snprintf(a->id,sizeof(a->id),"%s",id);snprintf(a->title,sizeof(a->title),"%s",title);
    a->app=(DmApp){DM_API_VERSION,a->id,a->title,sizeof(WasmInstance),wasm_open,wasm_draw,wasm_click,wasm_text,wasm_key,wasm_close,!strcmp(id,"images")?".png,.jpg,.jpeg":!strcmp(id,"notepad")?".txt,.ini,.log,.json,.md":!strcmp(id,"pdf")?".pdf":!strcmp(id,"media")?".wav,.mp3,.m4a,.mp4,.mid,.midi,.rmi,.aif,.aiff,.aifc,.au,.snd,.voc":"",wasm_tick};
    IM3Environment env=m3_NewEnvironment(); IM3Module module=NULL; IM3Runtime runtime=NULL;
    M3Result error=env?m3_ParseModule(env,&module,a->bytes,(uint32_t)a->size):m3Err_mallocFailed;
    if(!error) runtime=m3_NewRuntime(env,64*1024,NULL);
    if(!error&&!runtime) error=m3Err_mallocFailed;
    int loaded=0;
    if(!error){error=m3_LoadModule(runtime,module);if(!error)loaded=1;}
    if(!error) error=link_host_imports(module);
    if(!error) error=m3_CompileModule(module);
    IM3Function fn=NULL;
    const char *required[]={"dm_app_abi_version","dm_app_init","dm_app_draw","dm_app_click","dm_app_close"};
    for(unsigned i=0;!error&&i<sizeof(required)/sizeof(required[0]);i++) error=m3_FindFunction(&fn,runtime,required[i]);
    if(runtime)m3_FreeRuntime(runtime);
    if(env)m3_FreeEnvironment(env);
    if(error){fprintf(stderr,"WASM app %s rejected: %s\n",id,error);if(module&&!loaded)m3_FreeModule(module);free(a->bytes);memset(a,0,sizeof(*a));return -1;}
    if(dm_register_app(&a->app)<0){free(a->bytes);memset(a,0,sizeof(*a));return -1;}
    app_count++;
    return 0;
}
