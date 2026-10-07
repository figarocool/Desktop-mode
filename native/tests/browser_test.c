#define DM_PLUGIN_LINUX
#include "../apps/browser.c"
#include <assert.h>
#include <unistd.h>
static char stored[32769],clipboard[1024];
static void clip_set(const char*s){snprintf(clipboard,sizeof(clipboard),"%s",s);}
static const char*clip_get(void){return clipboard;}
static int text_width(const char*s){int n=0;for(;*s;s++)if((*s&0xc0)!=0x80)n++;return n*8;}
static void message(const char*s){(void)s;}
static void sysinfo(DmSystemInfo*i){memset(i,0,sizeof(*i));}
static int read_file(const char*p,char*b,size_t cap){(void)p;if(!stored[0])return -1;assert(strlen(stored)<cap);strcpy(b,stored);return strlen(b);}
static int write_file(const char*p,const void*b,size_t length,int exclusive){(void)p;(void)exclusive;assert(length<sizeof(stored));memcpy(stored,b,length);stored[length]=0;return 0;}
static void wait_page(DmWindow*w){Browser*b=w->state;for(int i=0;i<10000&&b->running;i++){tick(w,1);usleep(1000);}assert(!b->running);}
int main(int argc,char**argv){
 assert(argc<=2);static DmHostAPI host;host.text_width=text_width;host.status=message;host.system_info=sysinfo;host.fs_read=read_file;host.fs_write=write_file;host.clipboard_text_set=clip_set;host.clipboard_text_get=clip_get;dm_api=&host;
 DmWindow w={0};w.used=1;w.w=700;w.h=420;w.state=calloc(1,sizeof(Browser));assert(w.state);open_browser(&w,NULL);Browser*b=w.state;
 if(argc==2){navigate(b,argv[1],1);wait_page(&w);}
 else {
  strcpy(b->url,"https://example.test/page");b->size=0;
  const char*html="<html><head><meta charset=\"utf-8\"></head><body><p>Caffè &amp; tè</p><script>SECRET_SCRIPT</script><a href=\"/next\">Pagina successiva</a></body></html>";
  assert(receive((char*)html,1,strlen(html),b)==strlen(html));render_page(b,w.w-44,"text/html");
 }
 assert(b->count>0&&b->link_count==1);assert(strstr(b->url,"/page"));assert(strstr(b->links[0],"/next"));
 int text_found=0;for(int i=0;i<b->count;i++){assert(!strstr(b->rows[i].text,"SECRET_SCRIPT"));if(strstr(b->rows[i].text,"Caffè"))text_found=1;}assert(text_found);
 save_favorite(b);assert(b->favorite_count==1&&strstr(stored,"/page"));save_favorite(b);assert(b->favorite_count==1);
 if(argc==2){navigate(b,b->links[0],1);wait_page(&w);assert(b->history_count==2&&strstr(b->url,"/next"));}
 char saved[1024];strcpy(saved,b->url);navigate(b,"file:///etc/passwd",1);assert(!strcmp(b->url,saved)&&!b->running);
 b->size=BODY_LIMIT-1;assert(receive("xx",1,2,b)==0&&b->overflow);
 click(&w,25,80);assert(b->address_focus);text(&w,"example.test");assert(!strcmp(b->address,"example.test"));
 key(&w,DM_KEY_HOME);key(&w,DM_KEY_RIGHT|DM_KEY_SHIFT);key(&w,DM_KEY_RIGHT|DM_KEY_SHIFT);key(&w,DM_KEY_COPY);assert(!strcmp(clipboard,"ex"));
 key(&w,DM_KEY_CUT);assert(!strcmp(b->address,"ample.test"));key(&w,DM_KEY_PASTE);assert(!strcmp(b->address,"example.test"));
 key(&w,DM_KEY_SELECT_ALL);text(&w,"file:///etc/passwd");key(&w,DM_KEY_ENTER);assert(!strcmp(b->url,saved)&&!b->running);
 close_browser(&w);free(w.state);
 w.state=calloc(1,sizeof(Browser));open_browser(&w,NULL);b=w.state;assert(b->favorite_count==1);close_browser(&w);free(w.state);
 puts("PASS: UTF-8 HTML, relative link resolution, editable address/cut/copy/paste, persistent favorites, script exclusion, protocol restriction, response limit (HTTP integration requires sockets)");return 0;
}
