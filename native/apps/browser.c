/* Lightweight native HTML/text browser. No JavaScript engine or CSS layout. */
#include "../app-sdk/desktop_plugin.h"
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/uri.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define INK DM_COLOR(24,38,55,255)
#define BLUE DM_COLOR(25,74,160,255)
#define BODY_LIMIT (2*1024*1024)
#define FAVORITES "ux0:/data/desktop-mode/browser-favorites.txt"
typedef struct {
    char text[256];
    int link;
}
WebRow;
typedef struct {
    CURL *easy;
    CURLM *multi;
    char *body;
    size_t size;
    char url[1024],message[160],history[16][1024],favorites[32][1024],links[96][1024];
    WebRow rows[512];
    int count,link_count,offset,history_count,history_at,favorite_count,show_favorites;
    int running,overflow,address_focus,address_dragging,address_press_x;
    char address[1024];
    size_t address_cursor,address_anchor,address_start,address_press;
}
Browser;
#define TAB_LIMIT 4
typedef struct {Browser*tabs[TAB_LIMIT];int count,active,settings,prompt_tab;} BrowserTabs;
static int valid_url(const char*s) {
    return s&&(!strncasecmp(s,"http://",7)||!strncasecmp(s,"https://",8))&&!strchr(s,'\n')&&!strchr(s,'\r');
}
static void stop(Browser*b) {
    if(b->easy) {
        curl_multi_remove_handle(b->multi,b->easy);
        curl_easy_cleanup(b->easy);
        b->easy=NULL;
    }
    b->running=0;
}
static size_t receive(char*data,size_t size,size_t count,void*context) {
    Browser*b=context;
    size_t n=size*count;
    if(n>BODY_LIMIT-b->size) {
        b->overflow=1;
        return 0;
    }
    memcpy(b->body+b->size,data,n);
    b->size+=n;
    b->body[b->size]=0;
    return n;
}
static void navigate(Browser*b,const char*url,int remember) {
    char address[1024];
    if(strlen(url)>=sizeof(address)-(strstr(url,"://")?0:8)) {
        dm_status("Indirizzo troppo lungo");
        return;
    }
    if(!strstr(url,"://"))snprintf(address,sizeof(address),"https://%s",url);
    else snprintf(address,sizeof(address),"%s",url);
    if(!valid_url(address)) {
        dm_status("Il browser apre soltanto indirizzi HTTP e HTTPS");
        return;
    }
    stop(b);
    b->address_focus=b->address_dragging=0;
    b->size=b->count=b->link_count=b->offset=b->overflow=b->show_favorites=0;
    snprintf(b->url,sizeof(b->url),"%s",address);
    if(remember) {
        b->history_count=b->history_at+1;
        if(b->history_count>=16) {
            memmove(b->history,b->history+1,15*sizeof(b->history[0]));
            b->history_count=15;
        }
        snprintf(b->history[b->history_count],1024,"%s",address);
        b->history_at=b->history_count++;
    }
    b->easy=curl_easy_init();
    if(!b->easy) {
        strcpy(b->message,"Memoria insufficiente");
        return;
    }
    curl_easy_setopt(b->easy,CURLOPT_URL,b->url);
    curl_easy_setopt(b->easy,CURLOPT_WRITEFUNCTION,receive);
    curl_easy_setopt(b->easy,CURLOPT_WRITEDATA,b);
    curl_easy_setopt(b->easy,CURLOPT_FOLLOWLOCATION,1L);
    curl_easy_setopt(b->easy,CURLOPT_MAXREDIRS,5L);
    curl_easy_setopt(b->easy,CURLOPT_PROTOCOLS_STR,"http,https");
    curl_easy_setopt(b->easy,CURLOPT_REDIR_PROTOCOLS_STR,"http,https");
    curl_easy_setopt(b->easy,CURLOPT_CONNECTTIMEOUT,10L);
    curl_easy_setopt(b->easy,CURLOPT_TIMEOUT,30L);
    curl_easy_setopt(b->easy,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(b->easy,CURLOPT_USERAGENT,"DesktopMode/3 (PS Vita; text browser)");
    curl_easy_setopt(b->easy,CURLOPT_ACCEPT_ENCODING,"");
#ifdef DM_PLUGIN_LINUX
    curl_easy_setopt(b->easy,CURLOPT_CAINFO,"native/assets/cacert.pem");
#else
    curl_easy_setopt(b->easy,CURLOPT_CAINFO,"app0:/assets/cacert.pem");
#endif
    if(curl_multi_add_handle(b->multi,b->easy)!=CURLM_OK) {
        stop(b);
        strcpy(b->message,"Richiesta non avviata");
        return;
    }
    b->running=1;
    strcpy(b->message,"Connessione... Stop per annullare");
}
static void newline(Browser*b) {
    if(b->count<511&&b->rows[b->count].text[0])b->count++;
}
static void append(Browser*b,const char*text,int link,int width) {
    for(size_t i=0;text[i]&&b->count<511;) {
        unsigned char ch=text[i];
        size_t length=1;
        if(ch>=0xc2&&ch<0xe0)length=2;
        else if(ch>=0xe0&&ch<0xf0)length=3;
        else if(ch>=0xf0)length=4;
        for(size_t j=1;j<length;j++)if(!text[i+j]) {
            length=1;
            break;
        }
        WebRow*r=&b->rows[b->count];
        size_t used=strlen(r->text);
        char part[5];
        memcpy(part,text+i,length);
        part[length]=0;
        if(ch<128&&isspace(ch)) {
            strcpy(part," ");
            length=1;
            if(!used||r->text[used-1]==' ') {
                i++;
                continue;
            }
        }
        if(used+strlen(part)>=sizeof(r->text)-1||(used&&dm_text_width_raw(r->text)+dm_text_width_raw(part)>width)) {
            newline(b);
            continue;
        }
        strcat(r->text,part);
        if(link>=0)r->link=link;
        i+=length;
    }
}
static void visit(Browser*b,xmlNode*node,int link,int width) {
    for(;node&&b->count<511;node=node->next) {
        if(node->type==XML_TEXT_NODE) {
            append(b,(const char*)node->content,link,width);
            continue;
        }
        if(node->type!=XML_ELEMENT_NODE)continue;
        const char*tag=(const char*)node->name;
        if(!strcasecmp(tag,"script")||!strcasecmp(tag,"style")||!strcasecmp(tag,"head"))continue;
        int block=!strcasecmp(tag,"p")||!strcasecmp(tag,"div")||!strcasecmp(tag,"li")||!strcasecmp(tag,"br")||!strcasecmp(tag,"h1")||!strcasecmp(tag,"h2")||!strcasecmp(tag,"tr");
        if(block)newline(b);
        int child_link=link;
        if(!strcasecmp(tag,"a")) {
            xmlChar*href=xmlGetProp(node,(const xmlChar*)"href");
            if(href&&b->link_count<96) {
                xmlChar*absolute=xmlBuildURI(href,(const xmlChar*)b->url);
                if(absolute&&valid_url((const char*)absolute)&&strlen((const char*)absolute)<1024) {
                    newline(b);
                    child_link=b->link_count++;
                    strcpy(b->links[child_link],(const char*)absolute);
                }
                xmlFree(absolute);
            }
            xmlFree(href);
        }
        visit(b,node->children,child_link,width);
        if(block||child_link!=link)newline(b);
    }
}
static void render_page(Browser*b,int width,const char*type) {
    memset(b->rows,0,sizeof(b->rows));
    for(int i=0;i<512;i++)b->rows[i].link=-1;
    b->count=0;
    if(type&&strstr(type,"text/plain"))append(b,b->body,-1,width);
    else {
        htmlDocPtr doc=htmlReadMemory(b->body,(int)b->size,b->url,NULL,HTML_PARSE_NONET|HTML_PARSE_NOERROR|HTML_PARSE_NOWARNING|HTML_PARSE_RECOVER);
        if(doc) {
            visit(b,xmlDocGetRootElement(doc),-1,width);
            xmlFreeDoc(doc);
        }          else append(b,"Pagina non interpretabile",-1,width);
    }
    if(b->rows[b->count].text[0])b->count++;
}
static size_t address_next(Browser*b,size_t p) {
    if(b->address[p])p++;
    while((b->address[p]&0xc0)==0x80)p++;
    return p;
}
static size_t address_previous(Browser*b,size_t p) {
    if(p)p--;
    while(p&&(b->address[p]&0xc0)==0x80)p--;
    return p;
}
static int address_width(Browser*b,size_t a,size_t end) {
    char text[1024];
    memcpy(text,b->address+a,end-a);
    text[end-a]=0;
    return dm_text_width_raw(text);
}
static size_t address_point(Browser*b,int x) {
    size_t p=b->address_start;
    while(b->address[p]) {
        size_t next=address_next(b,p);
        if(x-16<(address_width(b,b->address_start,p)+address_width(b,b->address_start,next))/2)break;
        p=next;
    }
    return p;
}
static void address_bounds(Browser*b,size_t*a,size_t*end) {
    *a=b->address_cursor<b->address_anchor?b->address_cursor:b->address_anchor;
    *end=b->address_cursor>b->address_anchor?b->address_cursor:b->address_anchor;
}
static void address_erase(Browser*b) {
    size_t a,end;
    address_bounds(b,&a,&end);
    memmove(b->address+a,b->address+end,strlen(b->address+end)+1);
    b->address_cursor=b->address_anchor=a;
}
static void text(DmWindow*w,const char*t) {
    Browser*b=w->state;
    if(!b->address_focus)return;
    size_t a,end;
    address_bounds(b,&a,&end);
    size_t length=strlen(b->address),add=strlen(t);
    if(strchr(t,'\n')||strchr(t,'\r')||length-(end-a)+add>=sizeof(b->address)) {
        dm_status("Indirizzo troppo lungo o con caratteri non validi");
        return;
    }
    address_erase(b);
    length=strlen(b->address);
    memmove(b->address+b->address_cursor+add,b->address+b->address_cursor,length-b->address_cursor+1);
    memcpy(b->address+b->address_cursor,t,add);
    b->address_cursor+=add;
    b->address_anchor=b->address_cursor;
}
static void tick(DmWindow*w,unsigned elapsed) {
    (void)elapsed;
    Browser*b=w->state;
    if(b->address_dragging) {
        DmPointerState pointer;
        dm_pointer_state(w,&pointer);
        if(!pointer.held)b->address_dragging=0;
        else if(abs(pointer.x-b->address_press_x)>3) {
            b->address_anchor=b->address_press;
            b->address_cursor=address_point(b,pointer.x);
        }
    }
    if(!b->running)return;
    int running=0;
    CURLMcode result=curl_multi_perform(b->multi,&running);
    if(result!=CURLM_OK) {
        snprintf(b->message,sizeof(b->message),"%s",curl_multi_strerror(result));
        stop(b);
        return;
    }
    int pending;
    CURLMsg*m;
    while((m=curl_multi_info_read(b->multi,&pending))) {
        if(m->msg!=CURLMSG_DONE)continue;
        long status=0;
        char*url=NULL,*type=NULL;
        curl_easy_getinfo(b->easy,CURLINFO_RESPONSE_CODE,&status);
        curl_easy_getinfo(b->easy,CURLINFO_EFFECTIVE_URL,&url);
        curl_easy_getinfo(b->easy,CURLINFO_CONTENT_TYPE,&type);
        if(url)snprintf(b->url,sizeof(b->url),"%s",url);
        if(m->data.result!=CURLE_OK)snprintf(b->message,sizeof(b->message),"%s",b->overflow?"Pagina oltre 2 MiB: caricamento interrotto":curl_easy_strerror(m->data.result));
        else if(type&&strncasecmp(type,"text/",5)&&!strstr(type,"xhtml"))strcpy(b->message,"Questo tipo di contenuto non e visualizzabile");
        else {
            render_page(b,w->w-44,type);
            snprintf(b->message,sizeof(b->message),"HTTP %ld | %zu byte | %d link",status,b->size,b->link_count);
        }
        stop(b);
        break;
    }
}
static void open_browser(DmWindow*w,const char*arg) {
    Browser*b=w->state;
    static int initialized;
    if(!initialized) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        initialized=1;
    }
    DmSystemInfo info;
    dm_system_info(&info);
    b->history_at=-1;
    b->body=malloc(BODY_LIMIT+1);
    b->multi=curl_multi_init();
    strcpy(b->message,"Browser testuale: indirizzi, link e preferiti. CSS, script e form non supportati.");
    char stored[32769];
    if(dm_fs_read(FAVORITES,stored,sizeof(stored))>=0) {
        char*save,*line=strtok_r(stored,"\n",&save);
        while(line&&b->favorite_count<32) {
            if(valid_url(line)&&strlen(line)<1024)strcpy(b->favorites[b->favorite_count++],line);
            line=strtok_r(NULL,"\n",&save);
        }
    }
    if(!b->body||!b->multi) {
        dm_status("Memoria browser insufficiente");
        return;
    }
    if(arg&&*arg)navigate(b,arg,1);
    else {
        strcpy(b->body,"<h1>Desktop Mode Browser</h1><p>Premi Indirizzo per navigare su HTTP o HTTPS.</p><p>I link sono blu. Preferiti mostra gli indirizzi salvati; Aggiungi salva la pagina corrente.</p><p>Questo browser mostra testo e link. CSS, immagini, form e JavaScript non sono disponibili.</p>");
        b->size=strlen(b->body);
        render_page(b,w->w-44,"text/html");
    }
}
#ifndef DM_PLUGIN_LINUX
static void address_result(const char*t,void*ctx) {
    DmWindow*w=ctx;
    if(w->used){BrowserTabs*tabs=w->state;if(tabs->prompt_tab>=0&&tabs->prompt_tab<tabs->count)navigate(tabs->tabs[tabs->prompt_tab],t,1);}
}
#endif
static void save_favorite(Browser*b) {
    if(!valid_url(b->url)) {
        dm_status("Apri prima una pagina");
        return;
    }
    for(int i=0;i<b->favorite_count;i++)if(!strcmp(b->favorites[i],b->url)) {
        dm_status("Preferito gia presente");
        return;
    }
    if(b->favorite_count==32) {
        dm_status("Limite 32 preferiti");
        return;
    }
    strcpy(b->favorites[b->favorite_count++],b->url);
    char data[32769];
    data[0]=0;
    for(int i=0;i<b->favorite_count;i++) {
        strcat(data,b->favorites[i]);
        strcat(data,"\n");
    }
    if(dm_fs_write(FAVORITES,data,strlen(data),0)<0) {
        b->favorite_count--;
        dm_status("Preferiti non salvati");
    }      else dm_status("Preferito salvato");
}
static void draw(DmWindow*w) {
    Browser*b=w->state;
    const char*labels[]= {
        dm_localize("Indirizzo","Address","Direccion"),dm_localize("Indietro","Back","Atras"),dm_localize("Avanti","Forward","Adelante"),dm_localize("Ricarica","Reload","Recargar"),dm_localize("Preferiti","Bookmarks","Favoritos"),dm_localize("Aggiungi","Add","Anadir"),"Stop"
    };
    for(int i=0;i<7;i++) {
        dm_rect(w->x+10+i*96,w->y+36,92,30,DM_COLOR(208,222,237,255));
        dm_text(w->x+14+i*96,w->y+57,labels[i],INK);
    }
    dm_rect(w->x+10,w->y+70,w->w-80,26,b->address_focus?DM_COLOR(84,149,222,255):DM_COLOR(154,173,195,255));
    dm_rect(w->x+11,w->y+71,w->w-82,24,DM_COLOR(255,255,255,255));
    dm_rect(w->x+w->w-65,w->y+70,55,26,DM_COLOR(208,222,237,255));
    dm_text(w->x+w->w-57,w->y+89,"Vai",INK);
    char address[1024];
    if(b->address_focus) {
        size_t start=b->address_start;
        if(start>b->address_cursor)start=b->address_cursor;
        while(address_width(b,start,b->address_cursor)>w->w-100&&start<b->address_cursor)start=address_next(b,start);
        b->address_start=start;
        size_t end=start;
        while(b->address[end]) {
            size_t candidate=address_next(b,end);
            if(address_width(b,start,candidate)>w->w-100)break;
            end=candidate;
        }
        memcpy(address,b->address+start,end-start);
        address[end-start]=0;
        size_t sa,sb;
        address_bounds(b,&sa,&sb);
        if(sa<start)sa=start;
        if(sb>end)sb=end;
        if(sb>sa)dm_rect(w->x+16+address_width(b,start,sa),w->y+73,address_width(b,sa,sb),20,DM_COLOR(151,195,246,255));
        dm_text_raw(w->x+16,w->y+89,address,INK);
        dm_rect(w->x+16+address_width(b,start,b->address_cursor),w->y+73,1,20,INK);
    } else {
        snprintf(address,sizeof(address),"%s",b->url[0]?b->url:"Clicca qui e scrivi un indirizzo...");
        while(dm_text_width_raw(address)>w->w-100&&address[0]) {
            size_t length=strlen(address)-1;
            while(length&&(address[length]&0xc0)==0x80)length--;
            address[length]=0;
        }
        dm_text_raw(w->x+16,w->y+89,address,INK);
    }
    dm_rect(w->x+10,w->y+135,w->w-20,w->h-189,DM_COLOR(255,255,255,255));
    int visible=(w->h-193)/23;
    for(int row=0;row<visible;row++) {
        int i=b->offset+row;
        if(b->show_favorites) {
            if(i<b->favorite_count) {
                char label[90];
                snprintf(label,sizeof(label),"%.85s",b->favorites[i]);
                dm_text_raw(w->x+20,w->y+156+row*23,label,BLUE);
            }
        }          else if(i<b->count)dm_text_raw(w->x+20,w->y+156+row*23,b->rows[i].text,b->rows[i].link>=0?BLUE:INK);
    }
    char status[95];
    snprintf(status,sizeof(status),"%.90s",b->message);
    dm_text(w->x+15,w->y+w->h-20,status,INK);
}
static void focus_address(DmWindow*w) {
    Browser*b=w->state;
    if(!b->address_focus) {
        snprintf(b->address,sizeof(b->address),"%s",b->url);
        b->address_cursor=strlen(b->address);
        b->address_anchor=b->address_start=0;
    }
    b->address_focus=1;
#ifndef DM_PLUGIN_LINUX
    dm_prompt("Browser - indirizzo",b->address,address_result,w);
#endif
}
static void click(DmWindow*w,int x,int y) {
    Browser*b=w->state;
    if(!b->body||!b->multi)return;
    if(y>=70&&y<96&&x>=10&&x<w->w-70) {
        int focused=b->address_focus;
        focus_address(w);
        if(focused)b->address_cursor=b->address_anchor=address_point(b,x);
        b->address_dragging=1;
        b->address_press_x=x;
        b->address_press=address_point(b,x);
        return;
    }
    if(y>=70&&y<96&&x>=w->w-65&&x<w->w-10) {
        if(b->address_focus)navigate(b,b->address,1);
        return;
    }
    if(y>=36&&y<66&&x>=10&&x<682) {
        int i=(x-10)/96;
        if(i==0)focus_address(w);
        if(i==1&&b->history_at>0) {
            b->history_at--;
            navigate(b,b->history[b->history_at],0);
        }
        if(i==2&&b->history_at+1<b->history_count) {
            b->history_at++;
            navigate(b,b->history[b->history_at],0);
        }
        if(i==3&&b->url[0])navigate(b,b->url,0);
        if(i==4) {
            b->show_favorites=!b->show_favorites;
            b->offset=0;
        }
        if(i==5)save_favorite(b);
        if(i==6) {
            stop(b);
            strcpy(b->message,"Caricamento annullato");
        }
        return;
    }
    if(y>=135&&y<w->h-54) {
        b->address_focus=0;
        int row=b->offset+(y-135)/23;
        if(b->show_favorites&&row<b->favorite_count)navigate(b,b->favorites[row],1);
        else if(!b->show_favorites&&row<b->count&&b->rows[row].link>=0)navigate(b,b->links[b->rows[row].link],1);
    }
}
static void key(DmWindow*w,int k) {
    Browser*b=w->state;
    if(b->address_focus) {
        int extend=k&DM_KEY_SHIFT;
        k&=~DM_KEY_SHIFT;
        size_t a,end;
        address_bounds(b,&a,&end);
        if(k==DM_KEY_ENTER) {
            navigate(b,b->address,1);
            return;
        }
        if(k==DM_KEY_SELECT_ALL) {
            b->address_anchor=0;
            b->address_cursor=strlen(b->address);
            return;
        }
        if(k==DM_KEY_COPY||k==DM_KEY_CUT) {
            char copied[1024];
            memcpy(copied,b->address+a,end-a);
            copied[end-a]=0;
            if(end>a)dm_clipboard_text_set(copied);
            if(k==DM_KEY_CUT)address_erase(b);
            return;
        }
        if(k==DM_KEY_PASTE) {
            text(w,dm_clipboard_text_get());
            return;
        }
        if(k==DM_KEY_BACKSPACE||k==DM_KEY_DELETE) {
            if(a==end)b->address_anchor=k==DM_KEY_BACKSPACE?address_previous(b,b->address_cursor):address_next(b,b->address_cursor);
            address_erase(b);
            return;
        }
        if(k==DM_KEY_LEFT)b->address_cursor=(!extend&&a!=end)?a:address_previous(b,b->address_cursor);
        if(k==DM_KEY_RIGHT)b->address_cursor=(!extend&&a!=end)?end:address_next(b,b->address_cursor);
        if(k==DM_KEY_HOME)b->address_cursor=0;
        if(k==DM_KEY_END)b->address_cursor=strlen(b->address);
        if(!extend)b->address_anchor=b->address_cursor;
        return;
    }
    int total=b->show_favorites?b->favorite_count:b->count;
    int visible=(w->h-193)/23;
    if(k==DM_KEY_UP&&b->offset)b->offset--;
    if(k==DM_KEY_DOWN&&b->offset+visible<total)b->offset++;
}
static int close_browser(DmWindow*w) {
    Browser*b=w->state;
    stop(b);
    if(b->multi)curl_multi_cleanup(b->multi);
    free(b->body);
    return 1;
}
static void page_close(DmWindow*w,Browser*page){void*saved=w->state;w->state=page;close_browser(w);w->state=saved;}
static void page_draw(DmWindow*w,Browser*page){void*saved=w->state;w->state=page;draw(w);w->state=saved;}
static void create_tab(DmWindow*w,const char*arg){BrowserTabs*t=w->state;if(t->count==TAB_LIMIT){dm_status(dm_localize("Massimo 4 schede","Maximum 4 tabs","Maximo 4 pestanas"));return;}Browser*page=calloc(1,sizeof(*page));if(!page){dm_status("Memoria insufficiente");return;}void*saved=w->state;w->state=page;open_browser(w,arg);w->state=saved;if(!page->body||!page->multi){page_close(w,page);free(page);return;}if(t->count){Browser*previous=t->tabs[t->active];page->favorite_count=previous->favorite_count;memcpy(page->favorites,previous->favorites,sizeof(page->favorites));}t->tabs[t->count]=page;t->active=t->count++;}
static void tabs_open(DmWindow*w,const char*arg){BrowserTabs*t=w->state;t->prompt_tab=-1;create_tab(w,arg);}
static void tabs_tick(DmWindow*w,unsigned ms){BrowserTabs*t=w->state;for(int i=0;i<t->count;i++){void*saved=w->state;w->state=t->tabs[i];tick(w,ms);w->state=saved;}}
static void tabs_draw(DmWindow*w){BrowserTabs*t=w->state;if(!t->count)return;page_draw(w,t->tabs[t->active]);int width=(w->w-135)/TAB_LIMIT;for(int i=0;i<t->count;i++){Browser*b=t->tabs[i];dm_rect(w->x+10+i*width,w->y+102,width-3,27,i==t->active?DM_COLOR(158,203,247,255):DM_COLOR(208,222,237,255));char title[60];snprintf(title,sizeof(title),"%d %.18s",i+1,b->url[0]?b->url:"Desktop Mode");dm_text_raw(w->x+14+i*width,w->y+122,title,INK);}const char*labels[]={"+","X","..."};for(int i=0;i<3;i++){dm_rect(w->x+w->w-120+i*36,w->y+102,33,27,DM_COLOR(208,222,237,255));dm_text(w->x+w->w-111+i*36,w->y+122,labels[i],INK);}if(t->settings){dm_rect(w->x+100,w->y+155,w->w-200,165,DM_COLOR(232,241,250,255));dm_text(w->x+120,w->y+195,dm_localize("Renderer interno di Desktop Mode","Desktop Mode internal renderer","Motor interno de Desktop Mode"),INK);dm_text(w->x+120,w->y+240,dm_localize("Testo e link; fino a 4 schede indipendenti.","Text and links; up to 4 independent tabs.","Texto y enlaces; hasta 4 pestanas independientes."),INK);dm_text(w->x+120,w->y+285,dm_localize("Clicca per chiudere le impostazioni.","Click to close settings.","Pulsa para cerrar los ajustes."),INK);}}

static void tabs_click(DmWindow*w,int x,int y){BrowserTabs*t=w->state;if(!t->count)return;if(t->settings){t->settings=0;return;}if(y>=102&&y<129){if(x>=w->w-120&&x<w->w-12){int action=(x-(w->w-120))/36;if(action==0)create_tab(w,NULL);if(action==1){if(t->count==1){dm_status(dm_localize("Mantieni almeno una scheda aperta","Keep at least one tab open","Mantener al menos una pestana abierta"));return;}Browser*page=t->tabs[t->active];page_close(w,page);free(page);memmove(t->tabs+t->active,t->tabs+t->active+1,(t->count-t->active-1)*sizeof(*t->tabs));t->count--;if(t->active>=t->count)t->active=t->count-1;t->prompt_tab=-1;}if(action==2)t->settings=1;return;}int width=(w->w-135)/TAB_LIMIT,index=(x-10)/width;if(x>=10&&index<t->count){t->active=index;t->tabs[index]->address_dragging=0;}return;}t->prompt_tab=t->active;Browser*page=t->tabs[t->active];void*saved=w->state;w->state=page;click(w,x,y);w->state=saved;for(int i=0;i<t->count;i++)if(t->tabs[i]!=page){t->tabs[i]->favorite_count=page->favorite_count;memcpy(t->tabs[i]->favorites,page->favorites,sizeof(page->favorites));}}
static void tabs_text(DmWindow*w,const char*input){BrowserTabs*t=w->state;if(!t->count||t->settings)return;void*saved=w->state;w->state=t->tabs[t->active];text(w,input);w->state=saved;}
static void tabs_key(DmWindow*w,int code){BrowserTabs*t=w->state;if(!t->count||t->settings)return;void*saved=w->state;w->state=t->tabs[t->active];key(w,code);w->state=saved;}
static int tabs_close(DmWindow*w){BrowserTabs*t=w->state;for(int i=0;i<t->count;i++){page_close(w,t->tabs[i]);free(t->tabs[i]);}return 1;}
const DmApp dm_browser_app={DM_API_VERSION,"browser","Browser",sizeof(BrowserTabs),tabs_open,tabs_draw,tabs_click,tabs_text,tabs_key,tabs_close,0,tabs_tick};
DM_EXPORT_APP(dm_browser_app)
