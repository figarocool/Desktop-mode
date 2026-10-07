#include "../app-sdk/desktop_plugin.h"
#include <stdio.h>
#include <string.h>
#define INK DM_COLOR(26,38,51,255)
#define PAPER DM_COLOR(255,255,255,255)
typedef struct {
    char text[DM_TEXT_MAX],path[DM_PATH_MAX];
    size_t cursor,anchor;
    uint64_t rename_seen;
    int selecting,dragging;
    int dirty,scroll,scrollbar_drag;
}
Note;
static void saved_as(const char*path,int replace,void*ctx) {
    DmWindow*w=ctx;
    if(!w->used)return;
    Note*n=w->state;
    if(dm_fs_write(path,n->text,strlen(n->text),!replace)<0) {
        dm_status("Salvataggio fallito: destinazione non scrivibile o file modificato");
        return;
    }
    snprintf(n->path,sizeof(n->path),"%s",path);
    n->dirty=0;
    dm_status("Documento salvato nella cartella scelta");
}
static void save_as(DmWindow*w) {
    Note*n=w->state;
    dm_fs_path_update(n->path,sizeof(n->path),&n->rename_seen);
    char directory[DM_PATH_MAX];
    snprintf(directory,sizeof(directory),"%s",n->path[0]?n->path:"ux0:/data/");
    if(n->path[0])dm_fs_parent(directory);
    const char*filename=n->path[0]?strrchr(n->path,'/'):NULL;
    DmFileDialogOptions options= {
        DM_FILE_SAVE,"Notepad - Salva con nome",directory,filename?filename+1:"Documento.txt",".txt,.ini,.log,.json,.md"
    };
    if(dm_file_dialog(&options,saved_as,w)<0)dm_status("Un selettore file e gia aperto");
}
static void save(DmWindow*w) {
    Note*n=w->state;
    dm_fs_path_update(n->path,sizeof(n->path),&n->rename_seen);
    if(!n->path[0]) {
        save_as(w);
        return;
    }
    if(dm_fs_write(n->path,n->text,strlen(n->text),0)<0)dm_status("Salvataggio fallito");
    else {
        n->dirty=0;
        dm_status("Documento salvato");
    }
}
static size_t previous(Note*n,size_t p) {
    if(p)p--;
    while(p&&(n->text[p]&0xc0)==0x80)p--;
    return p;
}
static size_t next(Note*n,size_t p) {
    if(n->text[p])p++;
    while((n->text[p]&0xc0)==0x80)p++;
    return p;
}
static void bounds(Note*n,size_t*a,size_t*b) {
    *a=n->cursor<n->anchor?n->cursor:n->anchor;
    *b=n->cursor>n->anchor?n->cursor:n->anchor;
}
static void erase_selection(Note*n) {
    size_t a,b;
    bounds(n,&a,&b);
    if(a==b)return;
    memmove(n->text+a,n->text+b,strlen(n->text+b)+1);
    n->cursor=n->anchor=a;
    n->dirty=1;
}
static void insert(DmWindow*w,const char*t) {
    Note*n=w->state;
    size_t a,b;
    bounds(n,&a,&b);
    size_t len=strlen(n->text),add=strlen(t);
    if(len-(b-a)+add>=sizeof(n->text)) {
        dm_status("Documento pieno: limite 32 KiB");
        return;
    }
    erase_selection(n);
    len=strlen(n->text);
    memmove(n->text+n->cursor+add,n->text+n->cursor,len-n->cursor+1);
    memcpy(n->text+n->cursor,t,add);
    n->cursor+=add;
    n->anchor=n->cursor;
    n->dirty=1;
    n->selecting=0;
}
static void typed(const char*t,void*ctx) {
    DmWindow*w=ctx;
    if(w->used)insert(w,t);
}
static void open_note(DmWindow*w,const char*argument) {
    Note*n=w->state;
    dm_fs_path_update(n->path,sizeof(n->path),&n->rename_seen);
    if(argument&&*argument) {
        char content[DM_TEXT_MAX];
        int r=dm_fs_read(argument,content,sizeof(content));
        if(r<0||memchr(content,0,(size_t)(r<0?0:r))) {
            dm_status(r==-2?"File oltre 32 KiB: non aperto per evitare troncamenti":"Impossibile aprire: file non leggibile o binario");
            return;
        }
        snprintf(n->text,sizeof(n->text),"%s",content);
        snprintf(n->path,sizeof(n->path),"%s",argument);
        n->cursor=n->anchor=strlen(n->text);
        n->dirty=n->scroll=0;
    }
}
static void opened(const char*path,int replace,void*ctx) {
    (void)replace;
    DmWindow*w=ctx;
    if(w->used)open_note(w,path);
}
static void open_dialog(DmWindow*w) {
    Note*n=w->state;
    char directory[DM_PATH_MAX];
    snprintf(directory,sizeof(directory),"%s",n->path[0]?n->path:"ux0:/data/");
    if(n->path[0])dm_fs_parent(directory);
    DmFileDialogOptions o= {
        DM_FILE_OPEN,"Notepad - Apri",directory,"",".txt,.ini,.log,.json,.md"
    };
    if(dm_file_dialog(&o,opened,w)<0)dm_status("Un selettore file e gia aperto");
}
static void open_confirmed(int yes,void*ctx) {
    if(yes)open_dialog(ctx);
}
static void choose_file(DmWindow*w) {
    Note*n=w->state;
    if(n->dirty)dm_confirm("Modifiche non salvate","Aprire un altro file e scartare le modifiche?",open_confirmed,w);
    else open_dialog(w);
}
static void vertical(DmWindow*w,int direction,int extend);
static void key(DmWindow*w,int k) {
    Note*n=w->state;
    int extend=k&DM_KEY_SHIFT;
    k&=~DM_KEY_SHIFT;
    size_t a,b;
    bounds(n,&a,&b);
    if(k==DM_KEY_SAVE)save(w);
    else if(k==DM_KEY_SELECT_ALL) {
        n->anchor=0;
        n->cursor=strlen(n->text);
    }      else if(k==DM_KEY_COPY||k==DM_KEY_CUT) {
        if(a==b) {
            dm_status("Seleziona il testo da copiare");
            return;
        }
        char copied[DM_TEXT_MAX];
        memcpy(copied,n->text+a,b-a);
        copied[b-a]=0;
        dm_clipboard_text_set(copied);
        if(k==DM_KEY_CUT)erase_selection(n);
        dm_status(k==DM_KEY_CUT?"Selezione tagliata":"Selezione copiata");
    }   else if(k==DM_KEY_PASTE)insert(w,dm_clipboard_text_get());
    else if(k==DM_KEY_ENTER)insert(w,"\n");
    else if(k==DM_KEY_BACKSPACE||k==DM_KEY_DELETE) {
        if(a!=b)erase_selection(n);
        else {
            if(k==DM_KEY_BACKSPACE)n->anchor=previous(n,n->cursor);
            else n->anchor=next(n,n->cursor);
            erase_selection(n);
        }
    }   else if(k==DM_KEY_LEFT||k==DM_KEY_RIGHT||k==DM_KEY_HOME||k==DM_KEY_END) {
        if(k==DM_KEY_LEFT)n->cursor=(!extend&&a!=b)?a:previous(n,n->cursor);
        if(k==DM_KEY_RIGHT)n->cursor=(!extend&&a!=b)?b:next(n,n->cursor);
        if(k==DM_KEY_HOME) {
            while(n->cursor&&n->text[n->cursor-1]!='\n')n->cursor--;
        }
        if(k==DM_KEY_END) {
            while(n->text[n->cursor]&&n->text[n->cursor]!='\n')n->cursor++;
        }
        if(!extend&&!n->selecting)n->anchor=n->cursor;
    }  else if(k==DM_KEY_UP||k==DM_KEY_DOWN)vertical(w,k==DM_KEY_UP?-1:1,extend);
    else if(k==DM_KEY_SCROLL_UP&&n->scroll)n->scroll--;
    else if(k==DM_KEY_SCROLL_DOWN)n->scroll++;
}
/* One measured layout shared by drawing and pointer hit testing. Never split UTF-8. */ static size_t line_end(Note*n,size_t start,int width) {
    size_t end=start;
    char buf[4096];
    while(n->text[end]&&n->text[end]!='\n') {
        size_t candidate=next(n,end),len=candidate-start;
        if(len>=sizeof(buf))break;
        memcpy(buf,n->text+start,len);
        buf[len]=0;
        if(dm_text_width_raw(buf)>width&&end>start)break;
        end=candidate;
    }
    return end;
}
static int span_width(Note*n,size_t a,size_t b) {
    char buf[4096];
    memcpy(buf,n->text+a,b-a);
    buf[b-a]=0;
    return dm_text_width_raw(buf);
}
static size_t point(DmWindow*w,int x,int y) {
    Note*n=w->state;
    int wanted=n->scroll+(y-120)/23;
    if(wanted<0)wanted=0;
    size_t start=0;
    for(int row=0;;row++) {
        size_t end=line_end(n,start,w->w-58);
        if(row==wanted||!n->text[end]) {
            size_t p=start;
            while(p<end) {
                size_t q=next(n,p);
                if(x-20<(span_width(n,start,p)+span_width(n,start,q))/2)break;
                p=q;
            }
            return p;
        }
        start=end+(n->text[end]=='\n');
    }
}
static void vertical(DmWindow*w,int direction,int extend) {
    Note*n=w->state;
    size_t start=0;
    int line=0;
    for(;;line++) {
        size_t end=line_end(n,start,w->w-58);
        if(n->cursor<=end||!n->text[end])break;
        start=end+(n->text[end]=='\n');
    }
    int x=20+span_width(n,start,n->cursor),target=line+direction;
    if(target<0)target=0;
    n->cursor=point(w,x,120+(target-n->scroll)*23);
    if(!extend&&!n->selecting)n->anchor=n->cursor;
    if(target<n->scroll)n->scroll=target;
    int visible=(w->h-158)/23;
    if(target>=n->scroll+visible)n->scroll=target-visible+1;
}
static void draw(DmWindow*w) {
    Note*n=w->state;
    const char*labels[]= {
        "Tastiera","Apri","Salva","Salva come","Copia","Incolla"
    };
    for(int i=0;i<6;i++) {
        dm_rect(w->x+10+i*112,w->y+36,106,30,DM_COLOR(208,222,237,255));
        dm_text(w->x+16+i*112,w->y+57,labels[i],INK);
    }
    const char*extra[]= {
        n->selecting?"Fine selezione":"Seleziona","Seleziona tutto","Taglia"
    };
    for(int i=0;i<3;i++) {
        dm_rect(w->x+10+i*180,w->y+74,174,28,DM_COLOR(208,222,237,255));
        dm_text(w->x+16+i*180,w->y+94,extra[i],INK);
    }
    dm_rect(w->x+10,w->y+112,w->w-20,w->h-154,PAPER);
    size_t a,b;
    bounds(n,&a,&b);
    size_t start=0;int total_rows=0;
    int rows=(w->h-158)/23;
    for(int line=0;;line++) {
        size_t end=line_end(n,start,w->w-58);
        total_rows++;
        int row=line-n->scroll;
        if(row>=0&&row<rows) {
            size_t sa=a>start?a:start,sb=b<end?b:end;
            if(sb>sa)dm_rect(w->x+20+span_width(n,start,sa),w->y+120+row*23,span_width(n,sa,sb),22,DM_COLOR(151,195,246,255));
            if(b>end&&a<=end&&n->text[end]=='\n')dm_rect(w->x+20+span_width(n,start,end),w->y+120+row*23,8,22,DM_COLOR(151,195,246,255));
            char buf[4096];
            memcpy(buf,n->text+start,end-start);
            buf[end-start]=0;
            dm_text_raw(w->x+20,w->y+138+row*23,buf,INK);
            if(n->cursor>=start&&n->cursor<=end)dm_rect(w->x+20+span_width(n,start,n->cursor),w->y+120+row*23,2,20,INK);
        }
        if(!n->text[end]||row>=rows)break;
        start=end+(n->text[end]=='\n');
    }
    dm_scrollbar_draw(w->x+w->w-22,w->y+116,w->h-162,12,1,total_rows,rows,n->scroll);
    char status[120];
    snprintf(status,sizeof(status),dm_localize("%s%s | %zu byte | selezione %zu byte | L/R: scorri","%s%s | %zu bytes | selection %zu bytes | L/R: scroll","%s%s | %zu bytes | seleccion %zu bytes | L/R: desplazarse"),dm_localize(n->path[0]?"Documento":"Senza titolo",n->path[0]?"Document":"Untitled",n->path[0]?"Documento":"Sin titulo"),n->dirty?" *":"",strlen(n->text),b-a);
    dm_text_raw(w->x+15,w->y+w->h-19,status,INK);
}
static void click(DmWindow*w,int x,int y) {
    Note*n=w->state;
    if(y>=112&&y<w->h-42&&x>=w->w-25&&x<w->w-8){size_t p=0;int total=0,rows=(w->h-158)/23;for(;;){size_t end=line_end(n,p,w->w-58);total++;if(!n->text[end])break;p=end+(n->text[end]=='\n');}int max=total-rows;if(max<0)max=0;n->scroll=max*(y-116)/(w->h-162);if(n->scroll>max)n->scroll=max;n->scrollbar_drag=1;return;}
    if(y>=112&&y<w->h-42&&x>=10&&x<w->w-25) {
        n->cursor=point(w,x,y);
        if(!n->selecting)n->anchor=n->cursor;
        n->dragging=1;
        return;
    }
    if(y>=74&&y<=102&&x>=10&&x<550) {
        int i=(x-10)/180;
        if(i==0) {
            n->selecting=!n->selecting;
            if(n->selecting)n->anchor=n->cursor;
        }
        if(i==1)key(w,DM_KEY_SELECT_ALL);
        if(i==2)key(w,DM_KEY_CUT);
        return;
    }
    if(y<36||y>66||x<10||x>=682)return;
    int i=(x-10)/112;
    if(i==0) {
        char selected[DM_TEXT_MAX];
        size_t a,b;
        bounds(n,&a,&b);
        memcpy(selected,n->text+a,b-a);
        selected[b-a]=0;
        dm_prompt("Notepad - inserisci testo",selected,typed,w);
    }
    if(i==1)choose_file(w);
    if(i==2)save(w);
    if(i==3)save_as(w);
    if(i==4)key(w,DM_KEY_COPY);
    if(i==5)key(w,DM_KEY_PASTE);
}
static void tick(DmWindow*w,unsigned elapsed) {
    (void)elapsed;
    Note*n=w->state;
    dm_fs_path_update(n->path,sizeof(n->path),&n->rename_seen);
    DmPointerState p;
    dm_pointer_state(w,&p);
    if(n->scrollbar_drag){if(!p.held){n->scrollbar_drag=0;return;}size_t pos=0;int total=0,rows=(w->h-158)/23;for(;;){size_t end=line_end(n,pos,w->w-58);total++;if(!n->text[end])break;pos=end+(n->text[end]=='\n');}int max=total-rows;if(max<0)max=0;n->scroll=max*(p.y-116)/(w->h-162);if(n->scroll<0)n->scroll=0;if(n->scroll>max)n->scroll=max;return;}
    if(!n->dragging)return;
    if(!p.held) {
        n->dragging=0;
        return;
    }
    if(p.y<120&&n->scroll)n->scroll--;
    if(p.y>=w->h-42)n->scroll++;
    int y=p.y<120?120:p.y>=w->h-42?w->h-43:p.y;
    n->cursor=point(w,p.x,y);
}
static void close_result(int accepted,void*ctx) {
    DmWindow*w=ctx;
    if(!w->used)return;
    if(accepted) {
        ((Note*)w->state)->dirty=0;
        dm_close(w);
    }
}
static int close_note(DmWindow*w) {
    Note*n=w->state;
    if(n->dirty) {
        dm_confirm("Modifiche non salvate","Chiudere Notepad senza salvare le modifiche?",close_result,w);
        return 0;
    }
    return 1;
}
const DmApp dm_notepad_app= {
    DM_API_VERSION,"notepad","Notepad",sizeof(Note),open_note,draw,click,insert,key,close_note,".txt,.ini,.log,.json,.md",tick
};
DM_EXPORT_APP(dm_notepad_app)
