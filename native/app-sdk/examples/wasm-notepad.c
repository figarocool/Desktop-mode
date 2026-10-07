#include "../wasm_app.h"
#define DM_WIDGETS_WASM
#include "../dm_widgets.h"

#define TEXT_CAP 32768
static char text[TEXT_CAP+1],undo_text[TEXT_CAP+1],path[1024],input[1024],file_path[1024];
static uint64_t path_revision;
static uint32_t cursor,anchor,scroll;
static uint32_t undo_cursor;static int dirty,opening,status_visible,wrap_text,pending_new,undo_valid,dragging;
static int view_width,view_height;
static uint32_t length(void){uint32_t n=0;while(n<TEXT_CAP&&text[n])n++;return n;}
static uint32_t low(uint32_t a,uint32_t b){return a<b?a:b;}
static uint32_t high(uint32_t a,uint32_t b){return a>b?a:b;}
static void refresh_path(void){if(path[0])dm_host_fs_path_update(path,sizeof(path),&path_revision);}
static uint32_t visual_end(uint32_t start,uint32_t columns,uint32_t *next){uint32_t p=start,n=length();while(p<n&&text[p]!='\n'&&(!wrap_text||p-start<columns))p++;*next=p<n&&text[p]=='\n'?p+1:p;return p;}
static uint32_t visual_start(uint32_t row,uint32_t columns){uint32_t p=0,next=0;while(row--&&p<length())visual_end(p,columns,&next),p=next;return p;}
static uint32_t text_point(int32_t x,int32_t y){uint32_t columns=(uint32_t)(view_width-36)/8;if(!columns)columns=1;int row=(y-10)/18;if(row<0)row=0;uint32_t start=visual_start(scroll+(uint32_t)row,columns),next=0,end=visual_end(start,columns,&next);int col=x<=18?0:(x-18)/8;if(col<0)col=0;uint32_t p=start;while(col--&&p<end){p++;while(p<end&&(text[p]&0xc0)==0x80)p++;}return p;}
static void save_undo(void){uint32_t n=length();for(uint32_t i=0;i<=n;i++)undo_text[i]=text[i];undo_cursor=cursor;undo_valid=1;}
static void insert(const char *s,uint32_t n){uint32_t len=length(),a=low(cursor,anchor),b=high(cursor,anchor);if(len-(b-a)+n>=TEXT_CAP)return;save_undo();for(uint32_t i=len+1;i>b;i--)text[a+n+i-b-1]=text[i-1];for(uint32_t i=0;i<n;i++)text[a+i]=s[i];cursor=anchor=a+n;dirty=1;}
static void erase(void){uint32_t len=length(),a=low(cursor,anchor),b=high(cursor,anchor);if(a==b)return;save_undo();for(uint32_t i=b;i<=len;i++)text[a+i-b]=text[i];cursor=anchor=a;dirty=1;}
static void open_path(const char *p){int32_t n=dm_host_fs_read(p,text,TEXT_CAP);if(n<0){text[0]=0;return;}text[n]=0;for(uint32_t i=0;i<(uint32_t)n;i++)if(!text[i]){text[0]=0;return;}for(uint32_t i=0;p[i]&&i<sizeof(path)-1;i++)path[i]=p[i];path[sizeof(path)-1]=0;dm_host_fs_path_update(path,sizeof(path),&path_revision);cursor=anchor=(uint32_t)n;dirty=undo_valid=0;scroll=0;}
static void save(void){refresh_path();uint32_t n=length();if(path[0]){if(dm_host_fs_write(path,text,n,0)>=0)dirty=0;return;}opening=0;dm_host_file_dialog("Blocco note - Salva con nome",".txt,.ini,.log,.json,.md",1,file_path,sizeof(file_path));}
uint32_t dm_app_abi_version(void){return DM_WASM_APP_ABI_VERSION;}
uint32_t dm_app_argument_buffer(void){return (uint32_t)(uintptr_t)input;}
uint32_t dm_app_text_buffer(void){return (uint32_t)(uintptr_t)input;}
uint32_t dm_app_file_buffer(void){return (uint32_t)(uintptr_t)file_path;}
void dm_app_file_result(uint32_t n){if(n>=sizeof(file_path))return;file_path[n]=0;if(opening)open_path(file_path);else {uint32_t len=length();if(dm_host_fs_write(file_path,text,len,0)>=0){for(uint32_t i=0;file_path[i]&&i<sizeof(path)-1;i++)path[i]=file_path[i];path[sizeof(path)-1]=0;dm_host_fs_path_update(path,sizeof(path),&path_revision);dirty=0;}}}
void dm_app_init(const char *argument){text[0]=path[0]=file_path[0]=0;cursor=anchor=scroll=0;path_revision=0;dirty=opening=status_visible=wrap_text=pending_new=undo_valid=0;if(argument&&argument[0])open_path(argument);input[0]=0;}
void dm_app_text(uint32_t n){if(n>sizeof(input)-1)n=sizeof(input)-1;insert(input,n);}
void dm_app_key(int32_t key){refresh_path();int extend=(key&DM_WASM_KEY_SHIFT)!=0;key&=~DM_WASM_KEY_SHIFT;uint32_t len=length(),a=low(cursor,anchor),b=high(cursor,anchor);if(key==DM_WASM_KEY_SELECT_ALL){anchor=0;cursor=len;return;}if(key==DM_WASM_KEY_COPY||key==DM_WASM_KEY_CUT){if(a==b)return;char selected[TEXT_CAP];uint32_t n=b-a;for(uint32_t i=0;i<n;i++)selected[i]=text[a+i];selected[n]=0;dm_host_clipboard_set(selected);if(key==DM_WASM_KEY_CUT)erase();return;}if(key==DM_WASM_KEY_PASTE){int32_t n=dm_host_clipboard_get(input,sizeof(input));if(n>0)insert(input,(uint32_t)n);return;}if(key==DM_WASM_KEY_SAVE){save();return;}if(key==DM_WASM_KEY_ENTER){insert("\n",1);return;}if(key==DM_WASM_KEY_BACKSPACE||key==DM_WASM_KEY_DELETE){if(a!=b)erase();else if(key==DM_WASM_KEY_BACKSPACE&&cursor){uint32_t old=cursor;cursor--;while(cursor&&(text[cursor]&0xc0)==0x80)cursor--;anchor=old;erase();}else if(key==DM_WASM_KEY_DELETE&&cursor<len){anchor=cursor+1;while(text[anchor]&&(text[anchor]&0xc0)==0x80)anchor++;erase();}return;}if(key==DM_WASM_KEY_LEFT&&cursor){cursor--;while(cursor&&(text[cursor]&0xc0)==0x80)cursor--;}if(key==DM_WASM_KEY_RIGHT&&cursor<len){cursor++;while(cursor<len&&(text[cursor]&0xc0)==0x80)cursor++;}if(key==DM_WASM_KEY_HOME){while(cursor&&text[cursor-1]!='\n')cursor--;}if(key==DM_WASM_KEY_END){while(text[cursor]&&text[cursor]!='\n')cursor++;}if(key==DM_WASM_KEY_SCROLL_UP&&scroll)scroll--;if(key==DM_WASM_KEY_SCROLL_DOWN)scroll++;if(key==DM_WASM_KEY_LEFT||key==DM_WASM_KEY_RIGHT||key==DM_WASM_KEY_HOME||key==DM_WASM_KEY_END){if(!extend)anchor=cursor;}}
static void clear_document(void){text[0]=path[0]=file_path[0]=0;cursor=anchor=scroll=0;dirty=opening=undo_valid=0;}
static void open_dialog(void){opening=1;dm_host_file_dialog("Blocco note - Apri",".txt,.ini,.log,.json,.md",0,file_path,sizeof(file_path));}
void dm_app_event(int32_t type,int32_t result){if(type!=DM_WASM_EVENT_CONFIRM)return;int action=pending_new;pending_new=0;if(!result)return;if(action==DM_WASM_MENU_NEW)clear_document();else if(action==DM_WASM_MENU_OPEN)open_dialog();}
static void prepare_document_action(int action){if(!dirty){if(action==DM_WASM_MENU_NEW)clear_document();else open_dialog();return;}pending_new=action;dm_host_confirm("Blocco note","Le modifiche non salvate andranno perse. Continuare?");}
void dm_app_menu(int32_t command){switch(command){case DM_WASM_MENU_NEW:prepare_document_action(command);break;case DM_WASM_MENU_OPEN:prepare_document_action(command);break;case DM_WASM_MENU_SAVE:save();break;case DM_WASM_MENU_SAVE_AS:opening=0;dm_host_file_dialog("Blocco note - Salva con nome",".txt,.ini,.log,.json,.md",1,file_path,sizeof(file_path));break;case DM_WASM_MENU_UNDO:if(undo_valid){uint32_t n=0;while(n<TEXT_CAP&&undo_text[n])n++;for(uint32_t i=0;i<=n;i++)text[i]=undo_text[i];cursor=anchor=undo_cursor>n?n:undo_cursor;undo_valid=0;dirty=1;}break;case DM_WASM_MENU_CUT:dm_app_key(DM_WASM_KEY_CUT);break;case DM_WASM_MENU_COPY:dm_app_key(DM_WASM_KEY_COPY);break;case DM_WASM_MENU_PASTE:dm_app_key(DM_WASM_KEY_PASTE);break;case DM_WASM_MENU_SELECT_ALL:dm_app_key(DM_WASM_KEY_SELECT_ALL);break;case DM_WASM_MENU_WRAP:wrap_text=!wrap_text;break;case DM_WASM_MENU_STATUS:status_visible=!status_visible;break;case DM_WASM_MENU_INSERT_TEXT:dm_host_text_prompt("Blocco note - inserisci testo","",input,sizeof(input));break;}}
uint32_t dm_app_dirty(void){return dirty!=0;}
void dm_app_discard(void){dirty=0;}
void dm_app_draw(int32_t width,int32_t height){
    view_width=width;view_height=height;
    dmw_edit((DmWidgetRect){10,4,width-20,height-(status_visible?74:48)},0,DMW_FOCUSED,(int)cursor);
    uint32_t len=length(),columns=(uint32_t)(width-36)/8;if(!columns)columns=1;uint32_t pos=visual_start(scroll,columns);
    int maxrows=(height-(status_visible?88:52))/18;if(maxrows<1)maxrows=1;
    for(int line=0;line<maxrows&&pos<=len;line++){
        char buffer[160];uint32_t start=pos,n=0,next=pos;pos=visual_end(pos,columns,&next);while(start+n<pos&&n<sizeof(buffer)-1)buffer[n]=text[start+n],n++;buffer[n]=0;
        uint32_t a=low(cursor,anchor),b=high(cursor,anchor);if(b>start&&a<pos)dm_host_rect(16,10+line*18,(int32_t)(n*8+2),17,DM_WASM_COLOR_HIGHLIGHT);
        dm_host_text(18,24+line*18,buffer,DM_WASM_COLOR_TEXT);
        if(cursor>=start&&cursor<=pos)dm_host_rect(18+(int32_t)(cursor-start)*8,10+line*18,2,16,DM_WASM_COLOR_TEXT);
        if(next==pos&&pos>=len)break;pos=next;
    }
    if(status_visible){dm_host_rect(10,height-35,width-20,22,0xFFE6E9EDu);dm_host_text(16,height-19,dirty?"Modificato *":"Pronto",DM_WASM_COLOR_TEXT);dm_host_text(width-280,height-19,path[0]?path:"Senza titolo",DM_WASM_COLOR_TEXT);}
}
void dm_app_click(int32_t x,int32_t y,uint32_t buttons){if(!buttons)return;if(dmw_edit_click((DmWidgetRect){10,4,view_width-20,view_height-(status_visible?74:48)},x,y,0)){cursor=anchor=text_point(x,y);dragging=1;}}
void dm_app_tick(uint32_t ms){(void)ms;refresh_path();int32_t x=0,y=0,held=0;int focused=dm_host_pointer_state(&x,&y,&held);if(!focused||!held){dragging=0;return;}if(dragging&&y>=4&&y<view_height-(status_visible?40:12)&&x>=10&&x<view_width-10)cursor=text_point(x,y);}
void dm_app_close(void){}
