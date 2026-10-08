#include "desktop_ui.h"
#include "system_properties.h"
#include "file_jobs.h"
#include "app_manager.h"
#include "rename.h"
#include "icon_regions.h"
#include "device_ui.h"
#include "multi_file.h"
#include "preferences.h"
#include "app_runtime.h"
#include "drop_transfer.h"
#include "rdp_server.h"
#include "file_types.h"
#include "network_settings.h"
#include "file_icons.h"
#include "wasm_sandbox.h"
#include "updater.h"
#include <vita2d.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/kernel/processmgr.h>
#ifndef DESKTOP_PREVIEW
#include <psp2/sysmodule.h>
#endif
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#ifdef DESKTOP_PREVIEW
extern void desktop_pointer(int*,int*);
extern int desktop_input(char*,size_t,int*);
extern int desktop_import_dropped_file(const char*,char*,size_t);
static char external_drop_paths[128][DM_PATH_MAX],external_drop_target[DM_PATH_MAX];
static int external_drop_count;
#endif
#define C(r,g,b) DM_COLOR(r,g,b,255)
#define WHITE C(245,248,255)
#define DARK C(24,38,55)
#define STORE "ux0:/data/desktop-mode/"
#define DESK STORE "Desktop/"
#define MAX_ENTRIES 2048
static vita2d_pgf *font;
static vita2d_texture *icon_atlas,*walls[3],*custom;
static const char *names[]= {
    "Play OS Home","Risorse computer","Dispositivi PS","Documenti","Cestino","Giochi","Rete","Internet"
};
static const char *mounts[]= {
    "ux0:","uma0:","imc0:","ur0:","ud0:","vs0:","os0:","sa0:","gro0:","grw0:"
};
typedef struct {
    char name[256],actual[DM_PATH_MAX];
    int dir;
    unsigned long long size;
}
Entry;
typedef struct {
    Entry *entries;
    int count,offset,selected,click_valid,last_clicked,anchor;
    unsigned char marks[MAX_ENTRIES];
    uint64_t click_time;
    char path[DM_PATH_MAX],status[256],address[DM_PATH_MAX];
    int address_focus;
    size_t address_cursor,address_anchor;
}
Explorer;
static DmWindow windows[DM_MAX_WINDOWS];
static int order[DM_MAX_WINDOWS],order_count;
static const DmApp *registry[DM_APP_LIMIT];
static int app_count;
static int pointer_held,desktop_focused=1,selection_pad,selection_test_mods=-1;
static unsigned char desk_marks[32];
static int icon_start_x[32],icon_start_y[32];
static struct {int active,x,y,ctrl;DmWindow*window;unsigned char prior[MAX_ENTRIES];} rubber;
static int remote_selection_modifiers=-1;
static int selection_modifiers(void){if(selection_test_mods>=0)return selection_test_mods;
if(remote_selection_modifiers>=0)return remote_selection_modifiers;
#ifdef DESKTOP_PREVIEW
extern int desktop_selection_modifiers(void);return desktop_selection_modifiers();
#else
extern int dm_hid_modifiers(void);return dm_hid_modifiers()|selection_pad;
#endif
}
static void register_extensions(const DmApp*);
static int px=480,py=272,start,start_offset,wall,menu_x,menu_y,menu_count;
static DmWindow *drag_window,*context_window,*file_drag_window;
static char file_drag_path[DM_PATH_MAX];
static int file_drag_x,file_drag_y,file_drag_moved;
static int drag_x,drag_y;
static char custom_path[DM_PATH_MAX];
static char status[256],file_clipboard[DM_PATH_MAX],context_target[DM_PATH_MAX],text_clipboard[DM_TEXT_MAX];
static char (*multi_clipboard)[DM_PATH_MAX],(*multi_delete)[DM_PATH_MAX];
static int multi_clipboard_count,multi_delete_count;
static Entry desk_entries[24];
static int desk_count,fs_dirty,trash_has_items;
static uint64_t trash_checked;
static int file_icons_dirty;
void dm_files_changed(void) {
    fs_dirty=1;
    file_icons_dirty=1;
}
static struct {
    int active,shift,multiline;
    size_t cursor,anchor;
    char title[100],text[DM_TEXT_MAX];
    DmTextResult callback;
    void*context;
}
prompt;
enum {
    ACT_OPEN,ACT_COPY,ACT_PASTE,ACT_SHORTCUT,ACT_TEXT,ACT_FOLDER,ACT_REFRESH,ACT_COMPUTER,ACT_PERSONALIZE,ACT_NOTEPAD,ACT_TEXTCOPY,ACT_TEXTPASTE,ACT_SAVE,ACT_DELETE,ACT_EMPTY,ACT_RESTORE,ACT_TEXTCUT,ACT_SELECTALL,ACT_RENAME,ACT_PROPERTIES
};
static struct {
    const char*label;
    int action;
}
menu_items[16];
static int selection_paths(Explorer*,char(**out)[DM_PATH_MAX]);
static int entry_path(Explorer*,int,char*);
static const DmApp explorer_app;
static void refresh_desktop(void);
static void refresh_all(void);
static void explorer_scan(DmWindow*);
static void open_path(const char*,int);
static void launch_icon(int);
static void run_action(int);
static int icon_item_height(int,const char*);
static int desktop_icon_width(int);
static int desktop_icon_height(int);
typedef struct {
    char key[256];
    int x,y;
}
IconPosition;
static IconPosition positions[64];
static int position_count,selected_icon=-1,pending_icon=-1,icon_moved,last_icon=-1,icon_click_valid;
static int desktop_drop_hover=-1;
/* Keep a snapshot while dragging from Start.  App descriptors can belong to
 * runtime modules, so never retain a descriptor pointer across input frames. */
static char start_drag_id[64],start_drag_title[128];
static int start_drag_x,start_drag_y,start_drag_moved;
static uint64_t icon_click_time;
static int icon_press_x,icon_press_y,icon_original_x,icon_original_y;
static IconPosition*icon_position(int index) {
    char key[256];
    if(index<8)snprintf(key,sizeof(key),"builtin:%d",index);
    else snprintf(key,sizeof(key),"%s",desk_entries[index-8].name);
    for(int i=0;i<position_count;i++)if(!strcmp(positions[i].key,key))return &positions[i];
    if(position_count>=64)return &positions[63];
    IconPosition*p=&positions[position_count++];
    snprintf(p->key,sizeof(p->key),"%s",key);
    p->x=index<8?22+(index%2)*115:270+((index-8)%6)*105;
    p->y=index<8?25+(index/2)*112:25+((index-8)/6)*105;
    return p;
}
static void save_positions(void) {
#ifdef DESKTOP_PREVIEW
    if(getenv("DESKTOP_SELF_TEST"))return;
#endif
    char buffer[32768];
    size_t used=0;
    for(int i=0;i<position_count;i++) {
        int n=snprintf(buffer+used,sizeof(buffer)-used,"%s\t%d\t%d\n",positions[i].key,positions[i].x,positions[i].y);
        if(n<0||(size_t)n>=sizeof(buffer)-used)break;
        used+=n;
    }
    if(dm_fs_write(STORE "positions.txt",buffer,used,0)<0)dm_status("Posizione modificata, salvataggio fallito");
}
static void load_positions(void) {
    char buffer[32768];
    if(dm_fs_read(STORE "positions.txt",buffer,sizeof(buffer))<0)return;
    char*save,*line=strtok_r(buffer,"\n",&save);
    while(line&&position_count<64) {
        IconPosition p;
        if(sscanf(line,"%255[^\t]\t%d\t%d",p.key,&p.x,&p.y)==3&&p.x>=0&&p.x<=854&&p.y>=0&&p.y<=408)positions[position_count++]=p;
        line=strtok_r(NULL,"\n",&save);
    }
}
static int over_trash(void) {
    IconPosition*trash=icon_position(4);
    if(px<trash->x||px>=trash->x+desktop_icon_width(4)||py<trash->y||py>=trash->y+desktop_icon_height(4))return 0;
    for(int i=0;i<DM_MAX_WINDOWS;i++) {
        DmWindow*w=&windows[i];
        if(w->used&&!w->minimized&&px>=w->x&&px<w->x+w->w&&py>=w->y&&py<w->y+w->h)return 0;
    }
    return 1;
}
static void drop_trash(const char*path) {
    context_window=NULL;
    snprintf(context_target,sizeof(context_target),"%s",path);
    run_action(ACT_DELETE);
}
static int explorer_drop_target(int x,int y,char*out) {
    for(int z=order_count-1;z>=0;z--) {
        DmWindow*w=&windows[order[z]];
        if(!w->used||w->minimized||w->app!=&explorer_app||x<w->x||x>=w->x+w->w||y<w->y||y>=w->y+w->h)continue;
        Explorer*e=w->state;
        if(x<w->x+170||y<w->y+100||y>=w->y+w->h-50||!e->path[0]||!strcmp(e->path,"trash:"))return 0;
        int row=e->offset+(y-w->y-100)/27;
        if(row<e->count&&e->entries[row].dir) {
            if(entry_path(e,row,out)<0)return 0;
        } else if(row>=e->count)snprintf(out,DM_PATH_MAX,"%s",e->path);
        else return 0;
        return dm_fs_is_directory(out)&&dm_fs_writable(out);
    }
    return 0;
}
static int desktop_folder_drop_target(int x,int y,char*out) {
    /* File Explorer drops on desktop folders must resolve to that folder,
       while a drop on desktop background keeps the existing Desktop target. */
    for(int i=0;i<desk_count;i++) {
        if(!desk_entries[i].dir)continue;
        IconPosition*p=icon_position(i+8);
        if(x<p->x||x>=p->x+desktop_icon_width(i+8)||y<p->y||y>=p->y+desktop_icon_height(i+8))continue;
        if(dm_fs_join(out,DM_PATH_MAX,DESK,desk_entries[i].name)<0)return 0;
        return dm_fs_is_directory(out)&&dm_fs_writable(out);
    }
    return 0;
}
#ifdef DESKTOP_PREVIEW
static int external_drop_target_at(int x,int y,char*out){
    for(int z=order_count-1;z>=0;z--){DmWindow*w=&windows[order[z]];if(!w->used||w->minimized||x<w->x||x>=w->x+w->w||y<w->y||y>=w->y+w->h)continue;if(w->app==&explorer_app)return explorer_drop_target(x,y,out);return 0;}
    return desktop_folder_drop_target(x,y,out);
}
#endif
static int desktop_folder_drop_index(int x,int y) {
    for(int i=0;i<desk_count;i++) {
        if(!desk_entries[i].dir)continue;
        IconPosition*p=icon_position(i+8);
        if(x>=p->x&&x<p->x+desktop_icon_width(i+8)&&y>=p->y&&y<p->y+desktop_icon_height(i+8))return i;
    }
    return -1;
}
static int desktop_marked_paths(char(**out)[DM_PATH_MAX]) {
    char(*paths)[DM_PATH_MAX]=malloc((size_t)(desk_count?desk_count:1)*DM_PATH_MAX);
    if(!paths)return 0;
    int count=0;
    for(int i=0;i<desk_count;i++)if(desk_marks[i+8]&&dm_fs_join(paths[count],DM_PATH_MAX,DESK,desk_entries[i].name)==0)count++;
    if(!count){free(paths);paths=NULL;}
    *out=paths;
    return count;
}
static void update_file_drag(int held) {
    if(!file_drag_window)return;
    if(!file_drag_window->used||prompt.active||dm_confirm_active()||dm_file_dialog_active()||dm_job_active()) {
        file_drag_window=NULL;
        return;
    }
    if(held) {
        if(abs(px-file_drag_x)>5||abs(py-file_drag_y)>5) {
            file_drag_moved=1;
            ((Explorer*)file_drag_window->state)->click_valid=0;
        }
        return;
    }
    DmWindow*dragged_window=file_drag_window;file_drag_window=NULL;
    if(file_drag_moved&&over_trash()){context_window=dragged_window;run_action(ACT_DELETE);return;}
    if(file_drag_moved&&py<504){
        char target[DM_PATH_MAX];int valid=explorer_drop_target(px,py,target),covered=0;
        for(int i=0;i<DM_MAX_WINDOWS;i++){DmWindow*w=&windows[i];if(w->used&&!w->minimized&&px>=w->x&&px<w->x+w->w&&py>=w->y&&py<w->y+w->h)covered=1;}
        if(!valid&&!covered)valid=desktop_folder_drop_target(px,py,target);
        if(valid||!covered){context_window=dragged_window;char(*paths)[DM_PATH_MAX]=NULL;int count=selection_paths(dragged_window->state,&paths);const char*dir=valid?target:DESK;if(count>0&&dm_drop_begin((const char(*)[DM_PATH_MAX])paths,count,dir)<0)dm_status("Trasferimento non disponibile");else if(count==0)dm_status("Seleziona uno o più file da trascinare");free(paths);}
    }

}
static void begin_icon(int i) {
    int mods=selection_modifiers();desktop_focused=1;
    if((mods&2)&&selected_icon>=0){int a=selected_icon<i?selected_icon:i,b=selected_icon>i?selected_icon:i;if(!(mods&1))memset(desk_marks,0,sizeof(desk_marks));for(int n=a;n<=b;n++)desk_marks[n]=1;icon_click_valid=0;}
    else if(mods&1){desk_marks[i]=!desk_marks[i];icon_click_valid=0;}
    else if(!desk_marks[i]){memset(desk_marks,0,sizeof(desk_marks));desk_marks[i]=1;}
    selected_icon=i;pending_icon=desk_marks[i]?i:-1;
    for(int n=0;n<desk_count+8;n++){IconPosition*p=icon_position(n);icon_start_x[n]=p->x;icon_start_y[n]=p->y;}
    icon_moved=0;
    icon_press_x=px;
    icon_press_y=py;
    IconPosition*p=icon_position(i);
    icon_original_x=p->x;
    icon_original_y=p->y;
    dm_status("");
}
static void update_icon_drag(int held) {
    if(pending_icon<0)return;
    if(held) {
        int dx=px-icon_press_x,dy=py-icon_press_y;
        if(abs(dx)>5||abs(dy)>5)icon_moved=1;
        if(icon_moved) {
            for(int n=0;n<desk_count+8;n++)if(desk_marks[n]){int ih=desktop_icon_height(n);if(dx<-icon_start_x[n])dx=-icon_start_x[n];if(dx>854-icon_start_x[n])dx=854-icon_start_x[n];if(dy<-icon_start_y[n])dy=-icon_start_y[n];if(dy>464-ih-icon_start_y[n])dy=464-ih-icon_start_y[n];}
            for(int n=0;n<desk_count+8;n++)if(desk_marks[n]){IconPosition*p=icon_position(n);p->x=icon_start_x[n]+dx;p->y=icon_start_y[n]+dy;if(p->x<0)p->x=0;if(p->x>854)p->x=854;if(p->y<0)p->y=0;if(p->y>464-desktop_icon_height(n))p->y=464-desktop_icon_height(n);}
            desktop_drop_hover=desktop_folder_drop_index(px,py);
            if(desktop_drop_hover>=0&&desk_marks[desktop_drop_hover+8])desktop_drop_hover=-1;
        }
        return;
    }
    int index=pending_icon;
    pending_icon=-1;
    if(icon_moved) {
        int target_index=desktop_folder_drop_index(px,py);
        if(target_index>=0&&!desk_marks[target_index+8]) {
            char target[DM_PATH_MAX];
            if(dm_fs_join(target,sizeof(target),DESK,desk_entries[target_index].name)==0&&dm_fs_writable(target)) {
                for(int n=0;n<desk_count+8;n++)if(desk_marks[n]){IconPosition*p=icon_position(n);p->x=icon_start_x[n];p->y=icon_start_y[n];}
                char(*paths)[DM_PATH_MAX]=NULL;int count=desktop_marked_paths(&paths);
                if(count>0) {
                    if(dm_drop_begin((const char(*)[DM_PATH_MAX])paths,count,target)<0)dm_status("Trasferimento non disponibile");
                    else {memset(desk_marks,0,sizeof(desk_marks));desk_marks[target_index+8]=1;selected_icon=target_index+8;}
                } else dm_status("Seleziona uno o più file da trascinare");
                free(paths);desktop_drop_hover=-1;icon_click_valid=0;return;
            }
        }
        if(index!=4&&over_trash()) {
            IconPosition*position=icon_position(index);
            position->x=icon_original_x;
            position->y=icon_original_y;
            for(int n=0;n<desk_count+8;n++)if(desk_marks[n]){IconPosition*p=icon_position(n);p->x=icon_start_x[n];p->y=icon_start_y[n];}
            icon_click_valid=0;
            if(index>=8&&index-8<desk_count) {
                char path[DM_PATH_MAX];
                if(!dm_fs_join(path,sizeof(path),DESK,desk_entries[index-8].name))drop_trash(path);
            }             else dm_status("Questa icona di sistema non puo essere eliminata");
            return;
        }
        save_positions();
        desktop_drop_hover=-1;
        icon_click_valid=0;
        return;
    }
    desktop_drop_hover=-1;
    if(selection_modifiers()){icon_click_valid=0;return;}
    int selected_count=0;for(int n=0;n<32;n++)selected_count+=desk_marks[n]!=0;
    int twice=!selection_modifiers()&&selected_count==1&&icon_click_valid&&last_icon==index&&dm_clock_ms()-icon_click_time<=450;
    if(selected_count>1){memset(desk_marks,0,sizeof(desk_marks));desk_marks[index]=1;}
    last_icon=index;
    icon_click_valid=1;
    icon_click_time=dm_clock_ms();
    if(twice) {
        icon_click_valid=0;
        if(index<8)launch_icon(index);
        else {
            char file[DM_PATH_MAX];
            if(!dm_fs_join(file,sizeof(file),DESK,desk_entries[index-8].name))open_path(file,0);
        }
    }
}
static int hit(int x,int y,int w,int h) {
    return px>=x&&px<x+w&&py>=y&&py<y+h;
}
void dm_rect(int x,int y,int w,int h,uint32_t color) {
    if(!dm_current_window&&x<=0&&y<=0&&x+w>=960&&y+h>=544){vita2d_draw_rectangle(0,0,960,544,color);return;}
    dm_ui_transform_rect(&x,&y,&w,&h);
    if(dm_current_window){int l=dm_current_window->x,t=dm_current_window->y,r=l+dm_current_window->w,b=t+dm_current_window->h;if(x<l){w-=l-x;x=l;}if(y<t){h-=t-y;y=t;}if(x+w>r)w=r-x;if(y+h>b)h=b-y;}
    if(w>0&&h>0)vita2d_draw_rectangle(x,y,w,h,color);
}
void dm_scrollbar_draw(int x,int y,int length,int thickness,int vertical,int content,int viewport,int offset) {
    if(length<=0||thickness<=0||content<=viewport||viewport<=0)return;
    if(offset<0)offset=0;
    if(offset>content-viewport)offset=content-viewport;
    dm_rect(x,y,vertical?thickness:length,vertical?length:thickness,DM_COLOR(220,231,241,255));
    int thumb=(int)((int64_t)length*viewport/content),minimum=thickness*2;
    if(thumb<minimum)thumb=minimum;
    if(thumb>length)thumb=length;
    int pos=(int)((int64_t)(length-thumb)*offset/(content-viewport));
    dm_rect(vertical?x:x+pos,vertical?y+pos:y,vertical?thickness:thumb,vertical?thumb:thickness,DM_COLOR(119,150,181,255));
    if(thickness>2&&thumb>2)dm_rect(vertical?x+1:x+pos,vertical?y+pos:y+1,vertical?thickness-2:thumb,vertical?thumb:thickness-2,DM_COLOR(157,184,210,255));
}
float dm_ui_scale(void){return dm_preferences.font_percent/85.f;}
void dm_ui_transform_rect(int*x,int*y,int*w,int*h){if(!dm_current_window)return;float s=dm_ui_scale();*x=dm_current_window->x+(int)((*x-dm_current_window->x)*s+.5f);*y=dm_current_window->y+(int)((*y-dm_current_window->y)*s+.5f);*w=(int)(*w*s+.5f);*h=(int)(*h*s+.5f);}
void dm_ui_untransform_window_pointer(DmWindow*window,int*x,int*y){if(!window)return;float s=dm_ui_scale();if(s<=0)return;*x=window->x+(int)((*x-window->x)/s+.5f);*y=window->y+(int)((*y-window->y)/s+.5f);}
void dm_line(int x1,int y1,int x2,int y2,uint32_t color){if(dm_current_window){int w=0,h=0;dm_ui_transform_rect(&x1,&y1,&w,&h);w=0;h=0;dm_ui_transform_rect(&x2,&y2,&w,&h);}vita2d_draw_line(x1,y1,x2,y2,color);}
void dm_text(int x,int y,const char*s,uint32_t color) {
    char clipped[1024];const char*shown=dm_ui_translate(s);if(dm_current_window){float scale=dm_ui_scale();int available=(int)((dm_current_window->x+dm_current_window->w-x)/scale)-8;if(available<0)available=0;size_t n=strlen(shown);if(n>=sizeof(clipped))n=sizeof(clipped)-1;memcpy(clipped,shown,n);clipped[n]=0;while(n&&dm_text_width(clipped)>available){do{n--;}while(n&&((unsigned char)clipped[n]&0xc0)==0x80);clipped[n]=0;}shown=clipped;}
    if(dm_current_window){float scale=dm_ui_scale();x=dm_current_window->x+(int)((x-dm_current_window->x)*scale+.5f);y=dm_current_window->y+(int)((y-dm_current_window->y)*scale+.5f);}
    if(dm_current_window&&(y<dm_current_window->y||y>dm_current_window->y+dm_current_window->h-2))return;
    vita2d_pgf_draw_text(font,x,y,color,dm_preferences.font_percent/100.f,shown);
}
void dm_text_raw(int x,int y,const char*s,uint32_t color) {
    char clipped[1024];const char*shown=s;if(dm_current_window){float scale=dm_ui_scale();int available=(int)((dm_current_window->x+dm_current_window->w-x)/scale)-8;if(available<0)available=0;size_t n=strlen(s);if(n>=sizeof(clipped))n=sizeof(clipped)-1;memcpy(clipped,s,n);clipped[n]=0;while(n&&dm_text_width_raw(clipped)>available){do{n--;}while(n&&((unsigned char)clipped[n]&0xc0)==0x80);clipped[n]=0;}shown=clipped;}
    if(dm_current_window){float scale=dm_ui_scale();x=dm_current_window->x+(int)((x-dm_current_window->x)*scale+.5f);y=dm_current_window->y+(int)((y-dm_current_window->y)*scale+.5f);}
    if(dm_current_window&&(y<dm_current_window->y||y>dm_current_window->y+dm_current_window->h-2))return;
    vita2d_pgf_draw_text(font,x,y,color,dm_preferences.font_percent/100.f,shown);
}
int dm_text_width(const char*t) {
    return vita2d_pgf_text_width(font,dm_current_window?85.f/100.f:dm_preferences.font_percent/100.f,dm_ui_translate(t));
}
int dm_text_width_raw(const char*t) {
    return vita2d_pgf_text_width(font,dm_current_window?85.f/100.f:dm_preferences.font_percent/100.f,t);
}
void dm_text_raw_scaled(int x,int y,const char*s,uint32_t color,int percent) {
    char clipped[1024];const char*shown=s?s:"";if(percent<40)percent=40;if(percent>300)percent=300;
    if(dm_current_window){float ui=dm_ui_scale();int available=(int)((dm_current_window->x+dm_current_window->w-x)/ui)-8;if(available<0)available=0;size_t n=strlen(shown);if(n>=sizeof(clipped))n=sizeof(clipped)-1;memcpy(clipped,shown,n);clipped[n]=0;while(n&&dm_text_width_raw_scaled(clipped,percent)>available){do{n--;}while(n&&((unsigned char)clipped[n]&0xc0)==0x80);clipped[n]=0;}shown=clipped;}
    if(dm_current_window){float ui=dm_ui_scale();x=dm_current_window->x+(int)((x-dm_current_window->x)*ui+.5f);y=dm_current_window->y+(int)((y-dm_current_window->y)*ui+.5f);}
    if(dm_current_window&&(y<dm_current_window->y||y>dm_current_window->y+dm_current_window->h-2))return;
    float scale=(dm_current_window?85.f:dm_preferences.font_percent)/100.f*(float)percent/100.f;
    vita2d_pgf_draw_text(font,x,y,color,scale,shown);
}
int dm_text_width_raw_scaled(const char*s,int percent) {
    if(percent<40)percent=40;if(percent>300)percent=300;
    float scale=(dm_current_window?85.f:dm_preferences.font_percent)/100.f*(float)percent/100.f;
    return vita2d_pgf_text_width(font,scale,s?s:"");
}
void dm_text_center(int center,int y,const char*t,uint32_t color) {
    dm_text(center-dm_text_width(t)/2,y,t,color);
}
void dm_update_screen(const char *message,uint64_t current,uint64_t total) {
#ifndef DESKTOP_PREVIEW
    static unsigned marquee_frame;
    const uint32_t bg=DM_COLOR(15,35,66,255),panel=DM_COLOR(25,51,86,255);
    vita2d_start_drawing();
    vita2d_clear_screen();
    vita2d_draw_rectangle(0,0,960,544,bg);
    vita2d_draw_rectangle(0,0,960,7,DM_COLOR(68,145,218,255));
    vita2d_draw_rectangle(158,116,644,312,panel);
    vita2d_draw_rectangle(158,116,644,2,DM_COLOR(104,166,221,255));
    vita2d_draw_rectangle(158,116,6,312,DM_COLOR(74,139,201,255));
    const char *title="Desktop Mode";
    const char *status=message&&message[0]?message:"Controllo aggiornamenti...";
    const uint32_t white=DM_COLOR(245,250,255,255),light=DM_COLOR(218,232,248,255),muted=DM_COLOR(170,198,228,255);
    if(font){
        vita2d_pgf_draw_text(font,480-vita2d_pgf_text_width(font,0.85f,title)/2,190,white,0.85f,title);
        vita2d_pgf_draw_text(font,480-vita2d_pgf_text_width(font,0.70f,status)/2,245,light,0.70f,status);
    }
    const char *target="Controllo versione e aggiornamenti disponibili";
    if(message&&strstr(message,"Scaricamento app"))target="Destinazione: ux0:/data/desktop-mode/apps/";
    else if(message&&strstr(message,"aggiornamento del sistema"))target="Pacchetto completo per Desktop Mode (core e app incluse)";
    if(font)vita2d_pgf_draw_text(font,480-vita2d_pgf_text_width(font,0.58f,target)/2,275,muted,0.58f,target);
    vita2d_draw_rectangle(245,307,470,22,DM_COLOR(9,23,43,255));
    vita2d_draw_rectangle(247,309,466,18,DM_COLOR(41,68,101,255));
    if(total){
        uint64_t done=current>total?total:current;
        int width=(int)(466u*done/total);
        if(width>0)vita2d_draw_rectangle(247,309,width,18,DM_COLOR(67,159,239,255));
        char percent[48];snprintf(percent,sizeof(percent),"%u%%",(unsigned)(done*100/total));
        if(font)vita2d_pgf_draw_text(font,480-vita2d_pgf_text_width(font,0.58f,percent)/2,365,DM_COLOR(220,237,255,255),0.58f,percent);
    }else{
        int offset=(int)((marquee_frame++*9u)%560u)-94;
        if(offset<0)offset=0;
        if(offset>370)offset=370;
        vita2d_draw_rectangle(247+offset,309,96,18,DM_COLOR(67,159,239,255));
        if(font){const char *wait="Attendere...";vita2d_pgf_draw_text(font,480-vita2d_pgf_text_width(font,0.58f,wait)/2,365,DM_COLOR(220,237,255,255),0.58f,wait);}
    }
    const char *footer="Verifica sicura e download degli aggiornamenti";
    if(font)vita2d_pgf_draw_text(font,480-vita2d_pgf_text_width(font,0.52f,footer)/2,401,DM_COLOR(173,196,221,255),0.52f,footer);
    vita2d_end_drawing();
    vita2d_swap_buffers();
#else
    (void)message;(void)current;(void)total;
#endif
}
static void icon_label(int x,int y,int width,const char*name) {
    char text[256];
    snprintf(text,sizeof(text),"%s",name);
    char*shortcut=strstr(text,".dmlink");
    if(shortcut)*shortcut=0;
    if(dm_text_width_raw(text)<=width-4) {
        dm_text_raw(x+width/2-dm_text_width_raw(text)/2,y,text,WHITE);
        return;
    }
    size_t length=strlen(text),cut=length;
    char first[256];
    while(cut) {
        memcpy(first,text,cut);
        first[cut]=0;
        if(dm_text_width_raw(first)<=width-4)break;
        do {
            cut--;
        }
        while(cut&&((unsigned char)text[cut]&0xc0)==0x80);
    }
    size_t split=cut;
    while(split&&text[split]!=' ')split--;
    if(split)cut=split;
    memcpy(first,text,cut);
    first[cut]=0;
    dm_text_raw(x+width/2-dm_text_width_raw(first)/2,y,first,WHITE);
    const char*rest=text+cut;
    while(*rest==' ')rest++;
    char second[256];
    snprintf(second,sizeof(second),"%s",rest);
    if(dm_text_width_raw(second)>width-4) {
        size_t n=strlen(second);
        while(n) {
            do {
                n--;
            }
            while(n&&((unsigned char)second[n]&0xc0)==0x80);
            second[n]=0;
            char shortline[260];
            snprintf(shortline,sizeof(shortline),"%s...",second);
            if(dm_text_width_raw(shortline)<=width-4) {
                snprintf(second,sizeof(second),"%.255s",shortline);
                break;
            }
        }
    }
    dm_text_center(x+width/2,y+(17*dm_preferences.font_percent+42)/85,second,WHITE);
}
static int icon_item_height(int width,const char*name) {
    char label[256];
    snprintf(label,sizeof(label),"%s",name?name:"");
    char*shortcut=strstr(label,".dmlink");
    if(shortcut)*shortcut=0;
    int lines=dm_text_width_raw(label)<=width-4?1:2;
    int line_step=(17*dm_preferences.font_percent+42)/85;
    int descent=(4*dm_preferences.font_percent+42)/85;
    return dm_preferences.icon_size+21+(lines-1)*line_step+descent+1;
}
static int desktop_icon_width(int index){return index<8?106:100;}
static int desktop_icon_height(int index){
    if(index<0)return dm_preferences.icon_size+39;
    if(index<8)return icon_item_height(106,dm_ui_translate(names[index]));
    int file_index=index-8;
    return file_index<desk_count?icon_item_height(100,desk_entries[file_index].name):dm_preferences.icon_size+39;
}
static int start_button_width(void) {
    int width=dm_text_width("(P) START")+28;
    if(width<105)width=105;
    if(width>240)width=240;
    return width;
}
static int taskbar_tasks_x(void) { return 12+start_button_width()+8; }
static int task_width(void) {
    int available=680-taskbar_tasks_x();
    if(available<0)available=0;
    int width=order_count?available/order_count:190;
    if(width>190)width=190;
    return width;
}
void dm_status(const char*s) {
    /* Keep status for diagnostics and self-tests, but never cover app content. */
    snprintf(status,sizeof(status),"%s",s?s:"");
}
void dm_clipboard_text_set(const char*t) {
    snprintf(text_clipboard,sizeof(text_clipboard),"%s",t);
}
const char *dm_clipboard_text_get(void) {
    return text_clipboard;
}
void dm_prompt(const char*title,const char*initial,DmTextResult cb,void*ctx) {
    prompt.active=1;
    prompt.shift=0;
    prompt.multiline=!strncmp(title,"Notepad -",9);
    snprintf(prompt.title,sizeof(prompt.title),"%s",title);
    snprintf(prompt.text,sizeof(prompt.text),"%s",initial);
    prompt.cursor=strlen(prompt.text);
    prompt.anchor=0;
    prompt.callback=cb;
    prompt.context=ctx;
    menu_count=start=0;
}
#define START_ROWS 8
static const DmApp*start_app(int index){
 int sorted[DM_APP_LIMIT];for(int i=0;i<app_count;i++){sorted[i]=i;for(int j=i;j>0&&dm_ascii_casecmp(registry[sorted[j-1]]->title,registry[sorted[j]]->title)>0;j--){int t=sorted[j];sorted[j]=sorted[j-1];sorted[j-1]=t;}}
 return index>=0&&index<app_count?registry[sorted[index]]:NULL;
}
static void start_page(int direction){
 int last=app_count?(app_count-1)/START_ROWS*START_ROWS:0;
 start_offset+=direction*START_ROWS;if(start_offset<0)start_offset=last;if(start_offset>last)start_offset=0;
}
static int start_app_is_registered(const char*id){
 if(!id||!id[0])return 0;
 for(int i=0;i<app_count;i++)if(registry[i]&&registry[i]->id&&!strcmp(registry[i]->id,id))return 1;
 return 0;
}
static void create_start_shortcut(const char*id,const char*title){
 if(!id||!title||!start_app_is_registered(id)){dm_status("App non piu disponibile: collegamento annullato");return;}
 size_t id_len=strnlen(id,64);
 if(!id_len||id_len>=64){dm_status("ID app non valido: collegamento annullato");return;}
 for(size_t i=0;i<id_len;i++)if(!((id[i]>='a'&&id[i]<='z')||(id[i]>='A'&&id[i]<='Z')||(id[i]>='0'&&id[i]<='9')||id[i]=='_'||id[i]=='-')){dm_status("ID app non valido: collegamento annullato");return;}
 char base[220],name[256],path[DM_PATH_MAX],target[DM_PATH_MAX];size_t n=0;
 for(const unsigned char*p=(const unsigned char*)title;*p&&n<sizeof(base)-1;p++)base[n++]=(*p=='/'||*p=='\\'||*p==':'||*p=='\t'||*p<32)?'_':(char)*p;
 while(n&&base[n-1]==' ')n--;
 if(!n){memcpy(base,"Applicazione",12);n=12;}
 base[n]=0;
 path[0]=0;
 int path_ready=0;
 for(unsigned suffix=1;suffix<1000;suffix++){
  if(suffix==1)snprintf(name,sizeof(name),"%s.dmlink",base);else snprintf(name,sizeof(name),"%s (%u).dmlink",base,suffix);
  if(dm_fs_join(path,sizeof(path),DESK,name)<0)break;
  SceIoStat st;if(sceIoGetstat(path,&st)<0){path_ready=1;break;}
 }
 if(!path_ready||snprintf(target,sizeof(target),"app:%s",id)>=(int)sizeof(target)||dm_fs_write(path,target,strlen(target),1)<0){dm_status("Impossibile creare il collegamento sul desktop");return;}
 refresh_all();dm_status("Collegamento creato sul desktop");
}
static void update_start_drag(int held){
 if(!start_drag_id[0])return;
 if(held){if(abs(px-start_drag_x)>5||abs(py-start_drag_y)>5)start_drag_moved=1;return;}
 char id[sizeof(start_drag_id)],title[sizeof(start_drag_title)];
 snprintf(id,sizeof(id),"%s",start_drag_id);snprintf(title,sizeof(title),"%s",start_drag_title);
 int moved=start_drag_moved;start_drag_id[0]=start_drag_title[0]=0;start_drag_moved=0;start=0;
 if(!moved){if(start_app_is_registered(id))dm_launch(id,NULL);else dm_status("App non piu disponibile");return;}
 if(py>=504||hit(8,174,310,330))return;
 for(int i=0;i<DM_MAX_WINDOWS;i++){DmWindow*w=&windows[i];if(w->used&&!w->minimized&&px>=w->x&&px<w->x+w->w&&py>=w->y&&py<w->y+w->h)return;}
 create_start_shortcut(id,title);
}
static DmWindow *top_window(void) {
    for(int i=order_count-1;i>=0;i--)if(windows[order[i]].used&&!windows[order[i]].minimized)return &windows[order[i]];
    return NULL;
}
void dm_pointer_state(DmWindow*w,DmPointerState*p) {
    memset(p,0,sizeof(*p));
    int x=px,y=py;dm_ui_untransform_window_pointer(w,&x,&y);p->x=x-w->x;p->y=y-w->y;
    p->focused=!dm_saver_active()&&w==top_window()&&!start&&!menu_count&&!prompt.active&&!dm_file_dialog_active()&&!dm_confirm_active()&&!dm_job_active()&&!desktop_focused&&!dm_device_popup_active();
    p->held=pointer_held&&p->focused;
}
int dm_register_app(const DmApp*a) {
    if(!a||a->api_version!=DM_API_VERSION||!a->id||!a->title||!a->draw||app_count==DM_APP_LIMIT||a->state_size>512*1024)return -1;
    for(int i=0;i<app_count;i++)if(!strcmp(registry[i]->id,a->id))return -1;
    registry[app_count++]=a;
    register_extensions(a);
    return 0;
}
static struct {
    char extension[24],app[64];
}
associations[64];
static int association_count;
int dm_associate_extension(const char*ext,const char*app) {
    if(!dm_extension_valid(ext)||dm_extension_reserved(ext)||!app||strlen(app)>=64)return -1;
    int found=0;
    for(int i=0;i<app_count;i++)if(!strcmp(registry[i]->id,app))found=1;
    if(!found)return -1;
    for(int i=0;i<association_count;i++)if(!dm_ascii_casecmp(associations[i].extension,ext)) {
        snprintf(associations[i].app,64,"%s",app);
        return 0;
    }
    if(association_count>=64)return -1;
    snprintf(associations[association_count].extension,24,"%s",ext);
    snprintf(associations[association_count++].app,64,"%s",app);
    return 0;
}
int dm_association_count(void){return association_count;}
const char*dm_association_extension(int index){return index>=0&&index<association_count?associations[index].extension:NULL;}
const char*dm_association_default(const char*ext){for(int i=0;i<association_count;i++)if(!dm_ascii_casecmp(ext,associations[i].extension))return associations[i].app;return NULL;}
int dm_registered_count(void) {
    return app_count;
}
const DmApp*dm_registered_app(int index) {
    return index>=0&&index<app_count?registry[index]:NULL;
}
int dm_unregister_app(const char*id) {
    for(int i=0;i<DM_MAX_WINDOWS;i++)if(windows[i].used&&!strcmp(windows[i].app->id,id))return -1;
    int index=-1;
    for(int i=0;i<app_count;i++)if(!strcmp(registry[i]->id,id))index=i;
    if(index<0)return -1;
    memmove(registry+index,registry+index+1,(app_count-index-1)*sizeof(*registry));
    app_count--;
    for(int i=0;i<association_count;) {
        if(!strcmp(associations[i].app,id)) {
            memmove(associations+i,associations+i+1,(association_count-i-1)*sizeof(*associations));
            association_count--;
        }          else i++;
    }
    for(int i=0;i<app_count;i++)register_extensions(registry[i]);
    start_offset=0;
    return 0;
}
static void register_extensions(const DmApp*a) {
    if(!a->extensions)return;
    char list[512];
    snprintf(list,sizeof(list),"%s",a->extensions);
    char*save,*ext=strtok_r(list,",",&save);
    while(ext) {
        dm_associate_extension(ext,a->id);
        ext=strtok_r(NULL,",",&save);
    }
}
void dm_maximize(DmWindow*w) {
    if(!w||!w->used)return;
    if(w->maximized) {
        w->x=w->restore_x;
        w->y=w->restore_y;
        w->w=w->restore_w;
        w->h=w->restore_h;
        w->maximized=0;
    }     else {
        w->restore_x=w->x;
        w->restore_y=w->y;
        w->restore_w=w->w;
        w->restore_h=w->h;
        w->x=w->y=0;
        w->w=960;
        w->h=504;
        w->maximized=1;
    }
}
void dm_focus(DmWindow*w) {
    int id=(int)(w-windows);
    for(int i=0;i<order_count;i++)if(order[i]==id) {
        memmove(order+i,order+i+1,(order_count-i-1)*sizeof(*order));
        order[order_count-1]=id;
        break;
    }
    w->minimized=0;desktop_focused=0;
}
DmWindow *dm_launch(const char*id,const char*argument) {
    const DmApp*a=NULL;
    for(int i=0;i<app_count;i++)if(!strcmp(registry[i]->id,id))a=registry[i];
    if(!a) {
        DmInstalledApp apps[DM_APP_LIMIT];
        int count=dm_plugin_list(apps,DM_APP_LIMIT);
        for(int i=0;i<count;i++)if(apps[i].app&&!strcmp(apps[i].app->id,id)&&!apps[i].installed) {
            dm_status(dm_localize("App disinstallata: ripristinala da Applicazioni installate","App uninstalled: restore it from Installed apps","App desinstalada: restáurala desde Aplicaciones instaladas"));
            return NULL;
        }
        dm_status("App non registrata: modulo non caricato");
        return NULL;
    }
    for(int i=0;i<DM_MAX_WINDOWS;i++)if(!windows[i].used) {
        DmWindow*w=&windows[i];
        memset(w,0,sizeof(*w));
        w->state=calloc(1,a->state_size?a->state_size:1);
        if(!w->state) {
            dm_status("Memoria insufficiente");
            return NULL;
        }
        w->used=1;desktop_focused=0;
        w->app=a;
        w->x=200+order_count*12;
        w->y=45+order_count*8;
        if(w->x>260)w->x=260;
        if(w->y>84)w->y=84;
        w->w=700;
        w->h=420;
        order[order_count++]=i;
        DmWindow*prior=dm_current_window;dm_current_window=w;if(a->open)a->open(w,argument);dm_current_window=prior;
        start=menu_count=0;
        return w;
    }
    dm_status("Massimo 8 finestre: chiudine una");
    return NULL;
}
int dm_tasks(DmTaskInfo*out,int capacity){
 int count=0;for(int i=0;i<DM_MAX_WINDOWS&&count<capacity;i++)if(windows[i].used){out[count].window=&windows[i];out[count].minimized=windows[i].minimized;snprintf(out[count].title,sizeof(out[count].title),"%s",windows[i].app->title);count++;}return count;
}
int dm_close(DmWindow*w) {
    if(!w||!w->used)return 0;
    if(w->app->close&&!w->app->close(w))return 0;
    int id=w-windows;
    if(drag_window==w)drag_window=NULL;
    if(file_drag_window==w)file_drag_window=NULL;
    if(context_window==w)context_window=NULL;
    dm_runtime_closed(w);free(w->state);
    memset(w,0,sizeof(*w));
    for(int i=0;i<order_count;i++)if(order[i]==id) {
        memmove(order+i,order+i+1,(order_count-i-1)*sizeof(*order));
        order_count--;
        break;
    }
    if(!order_count)desktop_focused=1;
    return 1;
}
static int compare(const void*a,const void*b) {
    const Entry*x=a,*y=b;
    return x->dir!=y->dir?y->dir-x->dir:dm_ascii_casecmp(x->name,y->name);
}
static int read_dir(const char*p,Entry*out,int max) {
    int fd=sceIoDopen(p);
    if(fd<0)return -1;
    int count=0,n;
    SceIoDirent e;
    memset(&e,0,sizeof(e));
    while(count<max&&(n=sceIoDread(fd,&e))>0) {
        if(strcmp(e.d_name,".")&&strcmp(e.d_name,"..")) {
            Entry*v=&out[count++];
            snprintf(v->name,sizeof(v->name),"%s",e.d_name);
            v->dir=SCE_S_ISDIR(e.d_stat.st_mode);
            v->size=e.d_stat.st_size;
            v->actual[0]=0;
        }
        memset(&e,0,sizeof(e));
    }
    sceIoDclose(fd);
    qsort(out,count,sizeof(*out),compare);
    return count;
}
static void refresh_desktop(void) {
    file_icons_dirty=1;
    char marked_names[24][256];int marked_count=0;for(int i=0;i<desk_count;i++)if(desk_marks[i+8])snprintf(marked_names[marked_count++],256,"%.255s",desk_entries[i].name);
    char selected_name[256]="";
    if(selected_icon>=8&&selected_icon-8<desk_count)snprintf(selected_name,sizeof(selected_name),"%s",desk_entries[selected_icon-8].name);
    desk_count=read_dir(DESK,desk_entries,24);
    if(desk_count<0)desk_count=0;
    memset(desk_marks+8,0,24);for(int i=0;i<desk_count;i++)for(int j=0;j<marked_count;j++)if(!strcmp(desk_entries[i].name,marked_names[j]))desk_marks[i+8]=1;
    if(selected_name[0]) {
        selected_icon=-1;
        for(int i=0;i<desk_count;i++)if(!strcmp(selected_name,desk_entries[i].name))selected_icon=i+8;
    }
}
static int read_trash(Entry*out,int max) {
    int count=0;
    for(unsigned i=0;i<sizeof(mounts)/sizeof(*mounts);i++) {
        char files[DM_PATH_MAX],info[DM_PATH_MAX];
        if(dm_trash_directories(mounts[i],files,info))continue;
        int fd=sceIoDopen(files);
        if(fd<0)continue;
        SceIoDirent e;
        memset(&e,0,sizeof(e));
        while(count<max&&sceIoDread(fd,&e)>0) {
            if(strcmp(e.d_name,".")&&strcmp(e.d_name,"..")) {
                Entry*v=&out[count];
                memset(v,0,sizeof(*v));
                char metadata[DM_PATH_MAX],origin[DM_PATH_MAX];
                if(dm_fs_join(v->actual,sizeof(v->actual),files,e.d_name))continue;
                if(!dm_fs_join(metadata,sizeof(metadata),info,e.d_name)&&dm_fs_read(metadata,origin,sizeof(origin))>=0) {
                    const char*base=strrchr(origin,'/');
                    snprintf(v->name,sizeof(v->name),"%.255s",base?base+1:origin);
                }    else snprintf(v->name,sizeof(v->name),"%s",e.d_name);
                v->dir=SCE_S_ISDIR(e.d_stat.st_mode);
                v->size=e.d_stat.st_size;
                count++;
            }
            memset(&e,0,sizeof(e));
        }
        sceIoDclose(fd);
    }
    qsort(out,count,sizeof(*out),compare);
    return count;
}
static void explorer_scan(DmWindow*w) {
    file_icons_dirty=1;
    Explorer*e=w->state;
    e->count=e->offset=0;
    e->selected=e->anchor=-1;
    memset(e->marks,0,sizeof(e->marks));
    e->click_valid=0;
    e->status[0]=0;
    if(!e->entries)return;
    if(!strcmp(e->path,"trash:")) {
        e->count=read_trash(e->entries,MAX_ENTRIES);
        return;
    }
    if(!e->path[0]) {
        for(unsigned i=0;i<sizeof(mounts)/sizeof(*mounts);i++) {
            char p[32];
            snprintf(p,sizeof(p),"%s/",mounts[i]);
            if(dm_fs_is_directory(p)) {
                Entry*v=&e->entries[e->count++];
                memset(v,0,sizeof(*v));
                snprintf(v->name,sizeof(v->name),"%s",mounts[i]);
                v->dir=1;
            }
        }
        return;
    }
    e->count=read_dir(e->path,e->entries,MAX_ENTRIES);
    if(e->count<0) {
        e->count=0;
        snprintf(e->status,sizeof(e->status),"Cartella non accessibile: %.160s",e->path);
    }
}
static void explorer_open(DmWindow*w,const char*arg) {
    Explorer*e=w->state;
    e->entries=calloc(MAX_ENTRIES,sizeof(Entry));
    if(!e->entries) {
        dm_status("Memoria insufficiente per Esplora risorse");
        return;
    }
    snprintf(e->path,sizeof(e->path),"%s",arg?arg:"");
    explorer_scan(w);
}
static int explorer_close(DmWindow*w) {
    Explorer*e=w->state;
    free(e->entries);
    return 1;
}
static int entry_path(Explorer*e,int i,char*out) {
    if(i<0||i>=e->count)return -1;
    if(e->entries[i].actual[0]) {
        snprintf(out,DM_PATH_MAX,"%s",e->entries[i].actual);
        return 0;
    }
    if(!e->path[0])return snprintf(out,DM_PATH_MAX,"%s/",e->entries[i].name)>=DM_PATH_MAX?-1:0;
    return dm_fs_join(out,DM_PATH_MAX,e->path,e->entries[i].name);
}
static void explorer_activate(DmWindow*w) {
    Explorer*e=w->state;
    char full[DM_PATH_MAX];
    if(entry_path(e,e->selected,full))return;
    if(e->entries[e->selected].dir) {
        snprintf(e->path,sizeof(e->path),"%s",full);
        explorer_scan(w);
    }      else open_path(full,0);
}
static void explorer_address_go(DmWindow*w) {
    Explorer*e=w->state;
    char target[DM_PATH_MAX];
    snprintf(target,sizeof(target),"%s",e->address);
    if(!strcmp(target,"Computer"))target[0]=0;
    if(!strcmp(target,"Cestino"))strcpy(target,"trash:");
    size_t n=strlen(target);
    if(n&&target[n-1]==':'&&strcmp(target,"trash:")&&n+1<sizeof(target)){target[n++]='/';target[n]=0;}
    if(target[0]&&strcmp(target,"trash:")&&!dm_fs_is_directory(target)) {
        snprintf(e->status,sizeof(e->status),"Cartella non accessibile: %.160s",target);
        return;
    }
    snprintf(e->path,sizeof(e->path),"%s",target);
    e->address_focus=0;
    explorer_scan(w);
}
static void explorer_text(DmWindow*w,const char*text) {
    Explorer*e=w->state;
    if(!e->address_focus)return;
    size_t a=e->address_cursor<e->address_anchor?e->address_cursor:e->address_anchor;
    size_t b=e->address_cursor>e->address_anchor?e->address_cursor:e->address_anchor;
    size_t add=strlen(text),length=strlen(e->address);
    if(strchr(text,'\n')||strchr(text,'\r')||length-(b-a)+add>=sizeof(e->address))return;
    memmove(e->address+a+add,e->address+b,length-b+1);
    memcpy(e->address+a,text,add);
    e->address_cursor=e->address_anchor=a+add;
}
#ifndef DESKTOP_PREVIEW
static void explorer_address_result(const char*text,void*context) {
    DmWindow*w=context;
    Explorer*e=w->state;
    if(text){snprintf(e->address,sizeof(e->address),"%s",text);explorer_address_go(w);}
    else e->address_focus=0;
}
#endif
static void explorer_key(DmWindow*w,int key) {
    Explorer*e=w->state;
    if(e->address_focus) {
        int extend=key&DM_KEY_SHIFT;key&=~DM_KEY_SHIFT;
        size_t a=e->address_cursor<e->address_anchor?e->address_cursor:e->address_anchor;
        size_t b=e->address_cursor>e->address_anchor?e->address_cursor:e->address_anchor;
        if(key==DM_KEY_ENTER){explorer_address_go(w);return;}
        if(key==DM_KEY_SELECT_ALL){e->address_anchor=0;e->address_cursor=strlen(e->address);return;}
        if(key==DM_KEY_COPY||key==DM_KEY_CUT){char selected[DM_PATH_MAX];memcpy(selected,e->address+a,b-a);selected[b-a]=0;dm_clipboard_text_set(selected);if(key==DM_KEY_CUT)explorer_text(w,"");return;}
        if(key==DM_KEY_PASTE){explorer_text(w,dm_clipboard_text_get());return;}
        if(key==DM_KEY_BACKSPACE||key==DM_KEY_DELETE){
            if(a==b){if(key==DM_KEY_BACKSPACE&&a){a--;while(a&&(e->address[a]&0xc0)==0x80)a--;e->address_anchor=a;}
                else if(key==DM_KEY_DELETE&&e->address[b]){b++;while((e->address[b]&0xc0)==0x80)b++;e->address_anchor=b;}}
            explorer_text(w,"");return;
        }
        size_t cursor=e->address_cursor;
        if(key==DM_KEY_HOME)cursor=0;
        if(key==DM_KEY_END)cursor=strlen(e->address);
        if(key==DM_KEY_LEFT&&cursor){cursor--;while(cursor&&(e->address[cursor]&0xc0)==0x80)cursor--;}
        if(key==DM_KEY_RIGHT&&e->address[cursor]){cursor++;while((e->address[cursor]&0xc0)==0x80)cursor++;}
        e->address_cursor=cursor;if(!extend)e->address_anchor=cursor;
        return;
    }
    if(key==DM_KEY_SELECT_ALL){memset(e->marks,1,e->count);e->selected=e->count?0:-1;return;}
    if(key==DM_KEY_DELETE){context_window=w;run_action(ACT_DELETE);return;}
    if(key==DM_KEY_COPY||key==DM_KEY_PASTE) {
        context_window=w;
        run_action(key==DM_KEY_COPY?ACT_COPY:ACT_PASTE);
        return;
    }
    if(key==DM_KEY_DOWN) {
        e->offset+=10;
        if(e->offset>=e->count)e->offset=e->count>10?e->count-10:0;
    }      else if(key==DM_KEY_UP) {
        e->offset-=10;
        if(e->offset<0)e->offset=0;
    }      else if(key==DM_KEY_ENTER)explorer_activate(w);
}
static void explorer_click(DmWindow*w,int x,int y) {
    Explorer*e=w->state;
    if(y>=34&&y<68&&x>=86&&x<w->w-110) {
        if(!e->address_focus){snprintf(e->address,sizeof(e->address),"%s",e->path);e->address_cursor=strlen(e->address);e->address_anchor=0;}
        e->address_focus=1;
#ifndef DESKTOP_PREVIEW
        dm_prompt("Esplora risorse - percorso",e->address,explorer_address_result,w);
#endif
        return;
    }
    e->address_focus=0;
    if(y>=34&&y<68) {
        if(x<82) {
            dm_fs_parent(e->path);
            explorer_scan(w);
        }      else if(x>=w->w-110)explorer_scan(w);
        return;
    }
    if(x<160) {
        if(y>=225&&y<266) {
            strcpy(e->path,"trash:");
            explorer_scan(w);
            return;
        }
        if(y>=110&&y<150) {
            e->path[0]=0;
            explorer_scan(w);
        }
        if(y>=150&&y<188) {
            strcpy(e->path,"ux0:/data/");
            explorer_scan(w);
        }
        if(y>=188&&y<224) {
            strcpy(e->path,DESK);
            explorer_scan(w);
        }
        return;
    }
    if(x>=170&&y>=100&&y<w->h-50) {
        int i=e->offset+(y-100)/27;
        if(i<e->count) {
            int twice=e->click_valid&&e->last_clicked==i&&dm_clock_ms()-e->click_time<=450;
            int mods=selection_modifiers();
            if(mods&2&&e->anchor>=0){int a=e->anchor<i?e->anchor:i,b=e->anchor>i?e->anchor:i;if(!(mods&1))memset(e->marks,0,sizeof(e->marks));for(int n=a;n<=b;n++)e->marks[n]=1;}
            else if(mods&1)e->marks[i]=!e->marks[i];
            else if(!e->marks[i]){memset(e->marks,0,sizeof(e->marks));e->marks[i]=1;}
            if(!(mods&2))e->anchor=i;
            twice=twice&&!mods;e->selected=e->marks[i]?i:-1;if(e->selected<0)for(int n=0;n<e->count;n++)if(e->marks[n]){e->selected=n;break;}
            e->last_clicked=i;
            e->click_valid=!mods;
            e->click_time=dm_clock_ms();
            char msg[256];
            snprintf(msg,sizeof(msg),"Selezionato: %.200s",e->entries[i].name);
            dm_status(msg);
            if(twice) {
                file_drag_window=NULL;
                explorer_activate(w);
            }             else if(entry_path(e,i,file_drag_path)==0) {
                file_drag_window=w;
                file_drag_x=px;
                file_drag_y=py;
                file_drag_moved=0;
            }
        }
    }
}
static void explorer_draw(DmWindow*w) {
    Explorer*e=w->state;
    dm_rect(w->x+8,w->y+35,72,32,C(197,212,229));
    dm_text(w->x+12,w->y+57,"Indietro",DARK);
    dm_rect(w->x+86,w->y+35,w->w-200,32,e->address_focus?C(255,255,255):C(239,245,251));
    const char*address=e->address_focus?e->address:e->path[0]?e->path:"Computer";
    char visible[DM_PATH_MAX]={0};size_t start=0,end=strlen(address);
    if(e->address_focus){start=e->address_cursor;while(start){size_t prev=start-1;while(prev&&(address[prev]&0xc0)==0x80)prev--;snprintf(visible,sizeof(visible),"%.*s",(int)(e->address_cursor-prev),address+prev);if(dm_text_width_raw(visible)>w->w-222)break;start=prev;}}
    while(end>start){snprintf(visible,sizeof(visible),"%.*s",(int)(end-start),address+start);if(dm_text_width_raw(visible)<=w->w-218)break;end--;while(end>start&&(address[end]&0xc0)==0x80)end--;}
    if(e->address_focus){size_t a=e->address_cursor<e->address_anchor?e->address_cursor:e->address_anchor,b=e->address_cursor>e->address_anchor?e->address_cursor:e->address_anchor;if(a<start)a=start;if(b>end)b=end;if(b>a){char prefix[DM_PATH_MAX],selection[DM_PATH_MAX];snprintf(prefix,sizeof(prefix),"%.*s",(int)(a-start),address+start);snprintf(selection,sizeof(selection),"%.*s",(int)(b-a),address+a);dm_rect(w->x+92+dm_text_width_raw(prefix),w->y+39,dm_text_width_raw(selection),24,C(171,207,243));}char prefix[DM_PATH_MAX];snprintf(prefix,sizeof(prefix),"%.*s",(int)(e->address_cursor-start),address+start);dm_rect(w->x+92+dm_text_width_raw(prefix),w->y+40,1,22,DARK);}
    dm_text(w->x+92,w->y+57,visible,DARK);
    dm_text(w->x+w->w-100,w->y+57,"Aggiorna",DARK);
    dm_rect(w->x+8,w->y+75,150,w->h-122,C(215,228,239));
    const char*labels[]= {
        "Preferiti","Computer","Documenti","Desktop","Cestino"
    };
    for(int i=0;i<5;i++)dm_text(w->x+18,w->y+100+i*37,labels[i],DARK);
    dm_text(w->x+18,w->y+298,"L/R: scorri",DARK);
    dm_text(w->x+18,w->y+330,"[]: menu",DARK);
    dm_text(w->x+180,w->y+91,"Nome",DARK);
    dm_text(w->x+556,w->y+91,"Dimensione",DARK);
    for(int row=0;row<(w->h-130)/27&&e->offset+row<e->count;row++) {
        int i=e->offset+row;
        Entry*v=&e->entries[i];
        dm_rect(w->x+170,w->y+100+row*27,w->w-184,26,(e->marks[i]||e->selected==i)?C(171,207,243):row%2?C(224,234,244):C(246,249,253));
        char icon_path[DM_PATH_MAX];int icon_path_ok=!entry_path(e,i,icon_path);
        if(v->dir||!icon_path_ok||!dm_file_icon(icon_path,w->x+180,w->y+103+row*27,22))dm_text(w->x+180,w->y+120+row*27,v->dir?"[+]":" -",C(173,129,35));
        char label[48];
        snprintf(label,sizeof(label),"%.38s",v->name);
        dm_text_raw(w->x+214,w->y+120+row*27,label,DARK);
        char size[32];
        snprintf(size,sizeof(size),"%llu B",v->size);
        dm_text(w->x+557,w->y+120+row*27,v->dir?"Cartella":size,DARK);
    }
    char s[96];
    snprintf(s,sizeof(s),dm_localize("%d elementi | Tasto destro / Quadrato: operazioni","%d items | Right-click / Square: actions","%d elementos | Clic derecho / Cuadrado: acciones"),e->count);
    if(e->status[0])dm_text(w->x+15,w->y+w->h-19,e->status,DARK);else dm_text_raw(w->x+15,w->y+w->h-19,s,DARK);
}
static const DmApp explorer_app= {
    DM_API_VERSION,"explorer","Esplora risorse",sizeof(Explorer),explorer_open,explorer_draw,explorer_click,explorer_text,explorer_key,explorer_close,0,0
};
static void persist_wall(void) {
    char buf[DM_PATH_MAX+32];
    snprintf(buf,sizeof(buf),"%d\n%s",wall,custom_path);
    if(dm_fs_write(STORE "settings.txt",buf,strlen(buf),0)<0)dm_status("Sfondo applicato, impostazione non salvata");
}
static void personalize_draw(DmWindow*w) {
    dm_text(w->x+25,w->y+65,"Scegli lo sfondo del desktop",DARK);
    for(int i=0;i<3;i++)if(walls[i])dm_image_draw(walls[i],w->x+25+i*210,w->y+85,192,92);
    dm_rect(w->x+25,w->y+205,260,36,C(208,224,240));
    dm_text(w->x+35,w->y+230,"Scegli sfondo personalizzato",DARK);
    dm_text(w->x+25,w->y+260,"Gli sfondi inclusi vengono ricordati al prossimo avvio.",DARK);
}
static void wallpaper_chosen(const char*path,int replace,void*ctx){
 (void)replace;(void)ctx;vita2d_texture*t=dm_image_load(path);if(!t){dm_status("Immagine non valida");return;}if(custom)vita2d_free_texture(custom);custom=t;snprintf(custom_path,sizeof(custom_path),"%s",path);persist_wall();
}
static void personalize_click(DmWindow*w,int x,int y) {
    (void)w;
    if(x>=25&&x<285&&y>=205&&y<241){DmFileDialogOptions o={DM_FILE_OPEN,"Sfondo personalizzato",DESK,NULL,".png,.jpg,.jpeg"};dm_file_dialog(&o,wallpaper_chosen,NULL);return;}
    if(x>=25&&x<655&&y>=85&&y<180) {
        wall=(x-25)/210;
        if(custom) {
            vita2d_free_texture(custom);
            custom=NULL;
        }
        custom_path[0]=0;
        persist_wall();
    }
}
static const DmApp personalize_app= {
    DM_API_VERSION,"personalize","Personalizzazione",0,0,personalize_draw,personalize_click,0,0,0,0,0
};
static void about_draw(DmWindow*w) {
    const char*s[]={dm_localize("Desktop Mode App - C / Desktop API v3","Desktop Mode App - C / Desktop API v3","Desktop Mode App - C / Desktop API v3"),dm_localize("App interne: Esplora risorse, Notepad, Contatore.","Built-in apps: File Explorer, Notepad, Counter.","Apps incluidas: Explorador de archivos, Bloc de notas, Contador."),dm_localize("Tasto destro o Quadrato: menu contestuale.","Right-click or Square: context menu.","Clic derecho o Cuadrado: menu contextual."),dm_localize("Start: app interne e personalizzazione.","Start: built-in apps and personalization.","Inicio: apps incluidas y personalizacion."),dm_localize("Browser testuale e condivisioni di rete SMB2/3.","Text browser and SMB2/3 network shares.","Navegador de texto y recursos de red SMB2/3."),dm_localize("App esterne .dmapp compilate separatamente.","External .dmapp apps are built separately.","Las apps .dmapp externas se compilan por separado.")};
    for(int i=0;i<6;i++)dm_text(w->x+25,w->y+85+i*42,s[i],DARK);
}
static const DmApp about_app= {
    DM_API_VERSION,"about","Desktop Mode App",0,0,about_draw,0,0,0,0,0,0
};
static void launch_icon(int id) {
    if(id==4)dm_launch("explorer","trash:");
    else if(id==2)dm_launch("control",NULL);
    else if(id==1)dm_launch("explorer","");
    else if(id==3)dm_launch("explorer","ux0:/data/");
    else if(id==5)dm_launch("explorer","ux0:/app/");
    else if(id==6)dm_launch("network",NULL);
    else if(id==7)dm_launch("browser",NULL);
    else dm_launch("about",NULL);
}
static void open_path(const char*p,int depth) {
    if(!strcmp(p,"trash:")) {
        dm_launch("explorer","trash:");
        return;
    }
    if(depth>8) {
        dm_status("Collegamento circolare o troppo lungo");
        return;
    }
    if(dm_fs_is_directory(p)) {
        dm_launch("explorer",p);
        return;
    }
    const char*ext=strrchr(p,'.');
    if(ext&&!dm_ascii_casecmp(ext,".dmlink")) {
        char target[DM_PATH_MAX];
        if(dm_fs_read(p,target,sizeof(target))<0) {
            dm_status("Collegamento non valido");
            return;
        }
        target[strcspn(target,"\r\n")]=0;
        if(!strncmp(target,"app:",4))dm_launch(target+4,NULL);
        else if(!strcmp(target,"trash:"))open_path(target,depth+1);
        else if(strstr(target,":/")&&!strstr(target,"/../"))open_path(target,depth+1);
        else dm_status("Destinazione del collegamento non valida");
        return;
    }
    if(ext) {
        if(!dm_ascii_casecmp(ext,".dmapp")) {
            dm_load_plugin(p);
            return;
        }
        const char*associated=dm_association_app(ext);
        if(associated){dm_launch(associated,p);return;}
    }
    dm_status("Nessuna app associata a questo tipo di file");
}
void dm_open_file(const char*p) {
    open_path(p,0);
}
static char run_last[DM_PATH_MAX];
static void run_command(const char*input,void*context) {
    (void)context;
    if(!input)return;
    char target[DM_PATH_MAX];
    if(strlen(input)>=sizeof(target)){dm_status("Percorso troppo lungo");return;}
    snprintf(target,sizeof(target),"%s",input);
    char*begin=target;while(*begin==' '||*begin=='\t')begin++;
    if(begin!=target)memmove(target,begin,strlen(begin)+1);
    size_t n=strlen(target);while(n&&(target[n-1]==' '||target[n-1]=='\t'))target[--n]=0;
    if(n>=2&&target[0]=='"'&&target[n-1]=='"'){memmove(target,target+1,n-2);target[n-2]=0;n-=2;}
    if(!n)return;
    snprintf(run_last,sizeof(run_last),"%s",target);
    if(!dm_ascii_casecmp(target,"Computer")){dm_launch("explorer",NULL);return;}
    for(int i=0;i<app_count;i++)if(!strcmp(target,registry[i]->id)||!dm_ascii_casecmp(target,registry[i]->title)||(!strncmp(target,"app:",4)&&!strcmp(target+4,registry[i]->id))){dm_launch(registry[i]->id,NULL);return;}
    if(n&&target[n-1]==':'&&n+1<sizeof(target)){target[n++]='/';target[n]=0;}
    if(!strchr(target,':')){
        char relative[DM_PATH_MAX];snprintf(relative,sizeof(relative),"%s",target);
        DmWindow*active=top_window();const char*base=DESK;
        if(active&&active->app==&explorer_app){Explorer*e=active->state;if(e->path[0]&&strcmp(e->path,"trash:"))base=e->path;}
        if(snprintf(target,sizeof(target),"%s%s%s",base,base[strlen(base)-1]=='/'?"":"/",relative)>=(int)sizeof(target)){dm_status("Percorso troppo lungo");return;}
    }
    int valid=0;
    for(unsigned i=0;i<sizeof(mounts)/sizeof(*mounts);i++){size_t len=strlen(mounts[i]);if(!strncmp(target,mounts[i],len)&&target[len]=='/')valid=1;}
    if(!strncmp(target,"app0:/",6))valid=1;
    n=strlen(target);
    if(!valid||strstr(target,"/../")||strstr(target,"/./")||(n>=3&&!strcmp(target+n-3,"/.."))){dm_status("Percorso non valido: usa un'unita della console");return;}
    SceIoStat stat;
    if(sceIoGetstat(target,&stat)<0){dm_status("File o cartella non trovato");return;}
    const char*extension=strrchr(target,'.');
    if(extension&&!dm_ascii_casecmp(extension,".dmapp")&&!dm_fs_is_directory(target)){
        if(dm_load_plugin(target)<0)return;
        DmInstalledApp installed[DM_APP_LIMIT];int count=dm_plugin_list(installed,DM_APP_LIMIT);
        for(int i=0;i<count;i++)if(installed[i].installed&&!strcmp(installed[i].path,target)){dm_launch(installed[i].app->id,NULL);return;}
        return;
    }
    open_path(target,0);
}
static void show_run(void){dm_prompt(dm_localize("Esegui - cartella, file o app","Run - folder, file or app","Ejecutar - carpeta, archivo o app"),run_last,run_command,NULL);}
static void refresh_all(void) {
    fs_dirty=0;
    trash_has_items=dm_trash_has_items(NULL);
    trash_checked=dm_clock_ms();
    refresh_desktop();
    for(int i=0;i<DM_MAX_WINDOWS;i++)if(windows[i].used&&windows[i].app==&explorer_app)explorer_scan(&windows[i]);
}
/* Detect changes to mounted volumes; mounting USB devices remains the console's job. */
static unsigned mount_mask;
static int mounts_initialized,mount_dialog_dirty;
static uint64_t mount_checked;
static void poll_mounts(uint64_t now){
    if(mounts_initialized&&now-mount_checked<2000)return;
    mount_checked=now;unsigned mask=0;
    for(unsigned i=0;i<sizeof(mounts)/sizeof(*mounts);i++){char path[32];snprintf(path,sizeof(path),"%s/",mounts[i]);if(dm_fs_is_directory(path))mask|=1u<<i;}
    if(mounts_initialized&&mask!=mount_mask){
        dm_files_changed();mount_dialog_dirty=1;
        unsigned added=mask&~mount_mask;
        dm_status(dm_localize(added?"Nuova unita disponibile in Risorse computer":"Unita rimossa: elenco aggiornato",added?"New drive available in Computer":"Drive removed: list updated",added?"Nueva unidad disponible en Equipo":"Unidad retirada: lista actualizada"));
    }
    mount_mask=mask;mounts_initialized=1;
}
extern int dm_plugins_renamed(const char*,const char*);
int dm_files_renamed(const char*source,const char*destination){
 if(dm_plugins_renamed(source,destination)<0)return -1;
 for(int i=0;i<DM_MAX_WINDOWS;i++)if(windows[i].used&&windows[i].app==&explorer_app)dm_path_renamed(((Explorer*)windows[i].state)->path,DM_PATH_MAX,source,destination);
 dm_path_renamed(file_clipboard,sizeof(file_clipboard),source,destination);
 if(custom_path[0]){char before[DM_PATH_MAX];snprintf(before,sizeof(before),"%s",custom_path);dm_path_renamed(custom_path,sizeof(custom_path),source,destination);if(strcmp(before,custom_path))persist_wall();}
 if(!strncmp(source,DESK,strlen(DESK))&&!strchr(source+strlen(DESK),'/')){
  const char*old_name=source+strlen(DESK);const char*new_name=destination+strlen(DESK);
  for(int i=0;i<desk_count;i++)if(!strcmp(desk_entries[i].name,old_name))snprintf(desk_entries[i].name,sizeof(desk_entries[i].name),"%.255s",new_name);
  for(int i=0;i<position_count;i++)if(!strcmp(positions[i].key,old_name))snprintf(positions[i].key,sizeof(positions[i].key),"%s",new_name);
  save_positions();
 }
 return 0;
}
static const char*destination(void) {
    return context_window&&context_window->used&&context_window->app==&explorer_app?((Explorer*)context_window->state)->path:DESK;
}
static char creation_dir[DM_PATH_MAX];
static int creation_kind;
static void created(const char*value,void*ctx) {
    (void)ctx;
    char full[DM_PATH_MAX],name[256];
    snprintf(name,sizeof(name),"%s",value);
    if(creation_kind==ACT_TEXT&&!strrchr(name,'.')) {
        size_t n=strlen(name);
        if(n+4<sizeof(name))strcat(name,".txt");
    }
    if(creation_kind==ACT_SHORTCUT&&!strstr(name,".dmlink")) {
        size_t n=strlen(name);
        if(n+7<sizeof(name))strcat(name,".dmlink");
    }
    if(dm_fs_join(full,sizeof(full),creation_dir,name)) {
        dm_status("Nome non valido: evita /, \\, : e nomi vuoti");
        return;
    }
    int result=creation_kind==ACT_FOLDER?sceIoMkdir(full,0777):dm_fs_write(full,creation_kind==ACT_SHORTCUT?file_clipboard:"",creation_kind==ACT_SHORTCUT?strlen(file_clipboard):0,1);
    if(result<0) {
        dm_status("Creazione fallita: file esistente o cartella non scrivibile");
        return;
    }
    refresh_all();
    dm_status("Elemento creato");
    if(creation_kind==ACT_TEXT)dm_launch("notepad",full);
}
static void new_item(int kind) {
    const char*dir=kind==ACT_SHORTCUT?DESK:destination();
    if(!dir[0]||!dm_fs_writable(dir)) {
        dm_status("Scegli una cartella scrivibile");
        return;
    }
    snprintf(creation_dir,sizeof(creation_dir),"%s",dir);
    creation_kind=kind;
    if(kind==ACT_SHORTCUT&&!file_clipboard[0]) {
        dm_status("Seleziona un file/cartella o copia prima la destinazione");
        return;
    }
    dm_prompt(kind==ACT_TEXT?"Nuovo file di testo":kind==ACT_FOLDER?"Nuova cartella":"Nome collegamento sul desktop",kind==ACT_TEXT?"Documento.txt":kind==ACT_FOLDER?"Nuova cartella":"Collegamento.dmlink",created,NULL);
}
static void job_finished(int success,void*ctx) {
    (void)success;
    (void)ctx;
    refresh_all();
}
static char delete_target[DM_PATH_MAX];
static int delete_action;
static void delete_confirmed(int accepted,void*ctx) {
    (void)ctx;
    if(!accepted)return;
    int result=delete_action==ACT_EMPTY?dm_job_empty_trash(job_finished,NULL):delete_action==ACT_RESTORE?dm_job_restore(delete_target,job_finished,NULL):dm_job_trash(delete_target,job_finished,NULL);
    if(result<0)dm_status("Operazione non consentita su questa destinazione");
}
static void paste_file(void) {
    const char*dir=destination();
    if(!file_clipboard[0]||!dir[0]||!dm_fs_writable(dir)) {
        dm_status("Copia prima un elemento e scegli una cartella scrivibile");
        return;
    }
    char source[DM_PATH_MAX],target[DM_PATH_MAX],name[256];
    snprintf(source,sizeof(source),"%s",file_clipboard);
    size_t n=strlen(source);
    while(n&&source[n-1]=='/')source[--n]=0;
    const char*base=strrchr(source,'/');
    base=base?base+1:source;
    int success=0;
    for(int attempt=0;attempt<100;attempt++) {
        if(!attempt) {
            if(strlen(base)>=sizeof(name)) {
                dm_status("Nome file troppo lungo");
                return;
            }
            memcpy(name,base,strlen(base)+1);
        }                else {
            const char*ext=strrchr(base,'.');
            if(ext&&!dm_fs_is_directory(source))snprintf(name,sizeof(name),"%.*s - copia %d%.24s",(int)(ext-base)>190?190:(int)(ext-base),base,attempt,ext);
            else snprintf(name,sizeof(name),"%.220s - copia %d",base,attempt);
        }
        if(dm_fs_join(target,sizeof(target),dir,name))break;
        int fd=sceIoOpen(target,SCE_O_RDONLY,0);
        if(fd>=0) {
            sceIoClose(fd);
            continue;
        }
        if(dm_fs_is_directory(target))continue;
        if(dm_job_copy(source,target,job_finished,NULL)==0)success=1;
        break;
    }
    if(!success)dm_status("Copia non avviata: destinazione non valida");
}
static int selection_paths(Explorer*e,char(**out)[DM_PATH_MAX]){
 char(*paths)[DM_PATH_MAX]=malloc(MAX_ENTRIES*DM_PATH_MAX);if(!paths)return 0;int count=0;
 if(e){for(int i=0;i<e->count;i++)if(e->marks[i]&&!entry_path(e,i,paths[count]))count++;if(!count&&e->selected>=0&&!entry_path(e,e->selected,paths[count]))count++;}
 else {for(int i=0;i<desk_count;i++)if(desk_marks[i+8]&&!dm_fs_join(paths[count],DM_PATH_MAX,DESK,desk_entries[i].name))count++;if(!count&&context_target[0]&&strstr(context_target,":/")){snprintf(paths[count++],DM_PATH_MAX,"%s",context_target);}}
 if(!count){free(paths);paths=NULL;}*out=paths;return count;
}
static void multi_delete_confirmed(int accepted,void*ctx){(void)ctx;if(accepted&&dm_multi_trash((const char(*)[DM_PATH_MAX])multi_delete,multi_delete_count)<0)dm_status("Eliminazione multipla non avviata");free(multi_delete);multi_delete=NULL;multi_delete_count=0;}
static void run_action(int act) {
    Explorer*e=context_window&&context_window->used&&context_window->app==&explorer_app?context_window->state:NULL;
    char target[DM_PATH_MAX];
    if(act==ACT_SELECTALL&&(e||!context_window)){if(e){memset(e->marks,1,e->count);e->selected=e->count?0:-1;}else memset(desk_marks,1,desk_count+8);return;}
    if(act==ACT_COPY){free(multi_clipboard);multi_clipboard=NULL;multi_clipboard_count=selection_paths(e,&multi_clipboard);if(multi_clipboard_count){snprintf(file_clipboard,sizeof(file_clipboard),"%s",multi_clipboard[0]);char message[100];snprintf(message,sizeof(message),"%d elementi copiati negli appunti",multi_clipboard_count);dm_status(message);}else dm_status("Seleziona file o cartelle");return;}
    if(act==ACT_PASTE&&multi_clipboard_count>1){if(dm_multi_copy((const char(*)[DM_PATH_MAX])multi_clipboard,multi_clipboard_count,destination())<0)dm_status("Incolla multiplo non avviato");return;}
    if(act==ACT_DELETE){free(multi_delete);multi_delete=NULL;multi_delete_count=selection_paths(e,&multi_delete);if(multi_delete_count>1){char message[100];snprintf(message,sizeof(message),"Spostare %d elementi nel Cestino?",multi_delete_count);if(dm_confirm("Eliminazione multipla",message,multi_delete_confirmed,NULL)<0){free(multi_delete);multi_delete=NULL;multi_delete_count=0;}return;}if(multi_delete_count==1)snprintf(context_target,sizeof(context_target),"%s",multi_delete[0]);free(multi_delete);multi_delete=NULL;multi_delete_count=0;}
    if(act==ACT_DELETE||act==ACT_RESTORE||act==ACT_EMPTY) {
        delete_action=act;
        delete_target[0]=0;
        if(act!=ACT_EMPTY) {
            if(e)entry_path(e,e->selected,delete_target);
            else snprintf(delete_target,sizeof(delete_target),"%s",context_target);
            if(!delete_target[0]) {
                dm_status("Seleziona un elemento");
                return;
            }
        }
        char message[320];
        snprintf(message,sizeof(message),act==ACT_EMPTY?"Svuotare il Cestino di tutte le unita? I file saranno eliminati definitivamente.":act==ACT_RESTORE?"Ripristinare l'elemento nella sua cartella originale?":"Spostare nel Cestino: %.220s ?",delete_target);
        dm_confirm(act==ACT_EMPTY?"Svuota Cestino":act==ACT_RESTORE?"Ripristina":"Conferma eliminazione",message,delete_confirmed,NULL);
        return;
    }
    if(act==ACT_RENAME){
        int count=0;if(e){for(int i=0;i<e->count;i++)count+=e->marks[i]!=0;}else for(int i=0;i<32;i++)count+=desk_marks[i]!=0;if(count>1){dm_status("Per rinominare seleziona un solo elemento");return;}
        if(e&&entry_path(e,e->selected,target)==0)dm_rename_dialog(target);
        else if(context_target[0])dm_rename_dialog(context_target);
        return;
    }
    if(act==ACT_OPEN) {
        if(e)explorer_activate(context_window);
        else if(context_target[0])open_path(context_target,0);
    }
    if(act==ACT_COPY) {
        if(e&&entry_path(e,e->selected,target)==0)snprintf(file_clipboard,sizeof(file_clipboard),"%s",target);
        else if(!e&&context_target[0])snprintf(file_clipboard,sizeof(file_clipboard),"%s",context_target);
        dm_status(file_clipboard[0]?"Elemento copiato negli appunti":"Seleziona un elemento");
    }
    if(act==ACT_PASTE)paste_file();
    if(act==ACT_SHORTCUT) {
        if(e&&entry_path(e,e->selected,target)==0)snprintf(file_clipboard,sizeof(file_clipboard),"%s",target);
        else if(!e&&context_target[0])snprintf(file_clipboard,sizeof(file_clipboard),"%s",context_target);
        new_item(act);
    }
    if(act==ACT_TEXT||act==ACT_FOLDER)new_item(act);
    if(act==ACT_REFRESH) {
        dm_scan_plugins();
        refresh_all();
    }
    if(act==ACT_COMPUTER)dm_launch("explorer","");
    if(act==ACT_PROPERTIES)dm_launch("properties",NULL);
    if(act==ACT_PERSONALIZE)dm_launch("personalize",NULL);
    if(act==ACT_NOTEPAD)dm_launch("notepad",NULL);
    if(act==ACT_TEXTCOPY||act==ACT_TEXTPASTE||act==ACT_SAVE||act==ACT_TEXTCUT||act==ACT_SELECTALL) {
        DmWindow*w=context_window;
        if(w&&w->used&&w->app->key)w->app->key(w,act==ACT_TEXTCOPY?DM_KEY_COPY:act==ACT_TEXTPASTE?DM_KEY_PASTE:act==ACT_TEXTCUT?DM_KEY_CUT:act==ACT_SELECTALL?DM_KEY_SELECT_ALL:DM_KEY_SAVE);
    }
}
static void add_menu(const char*s,int action) {
    menu_items[menu_count].label=s;
    menu_items[menu_count++].action=action;
}
static void show_context(void) {
    if(dm_saver_wake()||dm_drop_active())return;
    menu_count=0;
    context_window=NULL;
    context_target[0]=0;
    for(int i=order_count-1;i>=0;i--) {
        DmWindow*w=&windows[order[i]];
        if(!w->minimized&&hit(w->x,w->y,w->w,w->h)) {
            context_window=w;
            dm_focus(w);
            break;
        }
    }
    if(context_window&&context_window->app==&explorer_app) {
        Explorer*e=context_window->state;
        if(hit(context_window->x+170,context_window->y+100,context_window->w-184,context_window->h-150)) {
            int row=(py-context_window->y-100)/27;
            int idx=e->offset+row;
            if(idx<e->count){if(!e->marks[idx]){memset(e->marks,0,sizeof(e->marks));e->marks[idx]=1;}e->selected=idx;}else e->selected=-1;
        }      else e->selected=-1;
        if(e->selected>=0) {
            add_menu("Apri",ACT_OPEN);
            add_menu("Copia",ACT_COPY);
            if(!strcmp(e->path,"trash:"))add_menu("Ripristina",ACT_RESTORE);
            else {add_menu("Rinomina",ACT_RENAME);add_menu("Elimina (Cestino)",ACT_DELETE);}
            add_menu("Crea collegamento sul desktop",ACT_SHORTCUT);
        }
        if(!strcmp(e->path,"trash:")) {
            add_menu("Svuota Cestino",ACT_EMPTY);
        }    else {
            add_menu("Incolla",ACT_PASTE);
            add_menu("Nuovo file di testo",ACT_TEXT);
            add_menu("Nuova cartella",ACT_FOLDER);
        }
        add_menu("Seleziona tutto",ACT_SELECTALL);
        add_menu("Aggiorna / cerca app",ACT_REFRESH);
        if(!e->path[0])add_menu("Proprietà",ACT_PROPERTIES);
    }      else if(context_window&&!strcmp(context_window->app->id,"notepad")) {
        add_menu("Seleziona tutto",ACT_SELECTALL);
        add_menu("Taglia",ACT_TEXTCUT);
        add_menu("Copia selezione",ACT_TEXTCOPY);
        add_menu("Incolla testo",ACT_TEXTPASTE);
        add_menu("Salva",ACT_SAVE);
    }      else {
        for(int i=0;i<desk_count;i++) {
            IconPosition*position=icon_position(i+8);
            int x=position->x,y=position->y;
            if(hit(x,y,desktop_icon_width(i+8),desktop_icon_height(i+8))) {
                if(!desk_marks[i+8]){memset(desk_marks,0,sizeof(desk_marks));desk_marks[i+8]=1;}selected_icon=i+8;
                if(!dm_fs_join(context_target,sizeof(context_target),DESK,desk_entries[i].name)) {
                    add_menu("Apri",ACT_OPEN);
                    add_menu("Copia",ACT_COPY);
                    add_menu("Rinomina",ACT_RENAME);
                    add_menu("Elimina (Cestino)",ACT_DELETE);
                }
                break;
            }
        }
        for(int i=0;i<8;i++) {
            IconPosition*position=icon_position(i);
            int x=position->x,y=position->y;
            if(hit(x,y,desktop_icon_width(i),desktop_icon_height(i))) {
                if(i==4) {
                    add_menu("Svuota Cestino",ACT_EMPTY);
                }
                snprintf(context_target,sizeof(context_target),"%s",i==1?"app:explorer":i==2?"app:control":i==4?"trash:":i==3?"ux0:/data/":i==5?"ux0:/app/":"app:about");
                if(i==1)add_menu("Proprietà",ACT_PROPERTIES);
                add_menu("Crea collegamento sul desktop",ACT_SHORTCUT);
                break;
            }
        }
        add_menu("Apri Computer",ACT_COMPUTER);
        add_menu("Apri Notepad",ACT_NOTEPAD);
        add_menu("Incolla",ACT_PASTE);
        add_menu("Nuovo file di testo",ACT_TEXT);
        add_menu("Nuova cartella",ACT_FOLDER);
        add_menu("Crea collegamento sul desktop",ACT_SHORTCUT);
        add_menu("Personalizza",ACT_PERSONALIZE);
        add_menu("Seleziona tutto",ACT_SELECTALL);
        add_menu("Aggiorna / cerca app",ACT_REFRESH);
    }
    menu_x=px>660?660:px;
    menu_y=py;
    if(menu_y+menu_count*30>504)menu_y=504-menu_count*30;
    start=0;
}
static const char *keyboard_rows[]= {
    "1234567890-_=","qwertyuiop[]","asdfghjkl;:'","zxcvbnm,./?!"
};
static void prompt_done(int accept);
static size_t prompt_prev(size_t pos){if(pos)pos--;while(pos&&(prompt.text[pos]&0xc0)==0x80)pos--;return pos;}
static size_t prompt_next(size_t pos){size_t n=strlen(prompt.text);if(pos<n){pos++;while(pos<n&&(prompt.text[pos]&0xc0)==0x80)pos++;}return pos;}
static void prompt_selection(size_t*a,size_t*b){*a=prompt.cursor<prompt.anchor?prompt.cursor:prompt.anchor;*b=prompt.cursor>prompt.anchor?prompt.cursor:prompt.anchor;}
static void prompt_replace(const char*t){
 size_t a,b;prompt_selection(&a,&b);size_t n=strlen(prompt.text),add=strlen(t);
 if(add>=sizeof(prompt.text)||n-(b-a)+add>=sizeof(prompt.text))return;
 memmove(prompt.text+a+add,prompt.text+b,n-b+1);memcpy(prompt.text+a,t,add);prompt.cursor=prompt.anchor=a+add;
}
static void prompt_key(int key){
 int extend=(key&DM_KEY_SHIFT)!=0;key&=~DM_KEY_SHIFT;
 size_t a,b,n=strlen(prompt.text);prompt_selection(&a,&b);
 if(key==DM_KEY_ENTER&&!prompt.multiline){prompt_done(1);return;}
 if(key==DM_KEY_SELECT_ALL){prompt.anchor=0;prompt.cursor=n;return;}
 if(key==DM_KEY_COPY||key==DM_KEY_CUT){if(!strncmp(prompt.title,"Password",8))return;if(a==b){a=0;b=n;}char selected[DM_TEXT_MAX];snprintf(selected,sizeof(selected),"%.*s",(int)(b-a),prompt.text+a);dm_clipboard_text_set(selected);if(key==DM_KEY_CUT)prompt_replace("");return;}
 if(key==DM_KEY_PASTE){prompt_replace(dm_clipboard_text_get());return;}
 if(key==DM_KEY_LEFT||key==DM_KEY_RIGHT||key==DM_KEY_HOME||key==DM_KEY_END){
  if(!extend&&a!=b){prompt.cursor=key==DM_KEY_LEFT?a:b;prompt.anchor=prompt.cursor;return;}
  if(key==DM_KEY_LEFT)prompt.cursor=prompt_prev(prompt.cursor);else if(key==DM_KEY_RIGHT)prompt.cursor=prompt_next(prompt.cursor);else if(key==DM_KEY_HOME)prompt.cursor=0;else prompt.cursor=n;
  if(!extend)prompt.anchor=prompt.cursor;
  return;
 }
 if(key==DM_KEY_BACKSPACE||key==DM_KEY_DELETE){
  if(a==b){if(key==DM_KEY_BACKSPACE)a=prompt_prev(prompt.cursor);else b=prompt_next(prompt.cursor);}
  if(a!=b){prompt.cursor=a;prompt.anchor=b;prompt_replace("");}return;
 }
 if(key==DM_KEY_ENTER&&prompt.multiline){prompt_replace("\n");return;}
}
static void prompt_text(const char*t) {
    prompt_replace(t);
}
static void prompt_done(int accept) {
    DmTextResult cb=prompt.callback;
    void*ctx=prompt.context;
    prompt.active=0;
    int password=!strncmp(prompt.title,"Password",8);
    if(accept&&cb)cb(prompt.text,ctx);
    if(password&&!prompt.active)memset(prompt.text,0,sizeof(prompt.text));
}
static void prompt_click(void) {
    if(!hit(110,105,740,365)) {
        prompt_done(0);
        return;
    }
    if(hit(250,425,160,34)) {
        prompt.text[0]=0;
        return;
    }
    if(hit(730,425,100,34)) {
        prompt_done(1);
        return;
    }
    if(hit(130,425,100,34)) {
        prompt_done(0);
        return;
    }
    for(int row=0;row<4;row++)for(size_t col=0;col<strlen(keyboard_rows[row]);col++)if(hit(130+(int)col*54,245+row*40,48,34)) {
        char t[2]= {
            keyboard_rows[row][col],0
        };
        if(prompt.shift&&t[0]>='a'&&t[0]<='z')t[0]-=32;
        prompt_text(t);
        return;
    }
    if(hit(130,405,105,16))prompt.shift=!prompt.shift;
    if(hit(250,405,210,16))prompt_text(" ");
    if(hit(475,405,160,16))prompt_key(DM_KEY_ENTER);
    if(hit(650,405,180,16))prompt_key(DM_KEY_BACKSPACE);
}
static void click(void) {
    if(dm_saver_wake())return;
    if(dm_drop_click(px,py))return;
    if(prompt.active) {
        prompt_click();
        return;
    }
    if(dm_confirm_active()) {
        dm_confirm_click(px,py);
        return;
    }
    if(dm_file_dialog_active()) {
        dm_file_dialog_click(px,py);
        return;
    }
    if(dm_job_active()) {
        if(hit(525,345,185,32))dm_job_cancel();
        return;
    }
    if(dm_runtime_tray_click(px,py))return;
    if(dm_device_tray_click(px,py))return;
    if(dm_network_tray_click(px,py))return;
    if(dm_clock_click(px,py))return;
    if(menu_count) {
        if(hit(menu_x,menu_y,300,menu_count*30)) {
            int action=menu_items[(py-menu_y)/30].action;
            menu_count=0;
            run_action(action);
        }      else menu_count=0;
        return;
    }
    if(hit(0,504,12+start_button_width()+4,40)) {
        start=!start;
        return;
    }
    if(start) {
        if(hit(8,174,310,START_ROWS*30)) {
            int idx=start_offset+(py-174)/30;const DmApp*app=start_app(idx);if(app&&app->id&&app->title){snprintf(start_drag_id,sizeof(start_drag_id),"%s",app->id);snprintf(start_drag_title,sizeof(start_drag_title),"%s",app->title);start_drag_x=px;start_drag_y=py;start_drag_moved=0;}
        } else if(hit(18,414,288,30))show_run();
        else if(hit(18,452,135,36))start_page(-1);
        else if(hit(171,452,135,36))start_page(1);
        else start=0;
        return;
    }
    for(int i=0;i<order_count;i++)if(hit(taskbar_tasks_x()+i*task_width(),504,task_width()-2,40)) {
        DmWindow*w=&windows[order[i]];
        if(top_window()==w&&!w->minimized)w->minimized=1;
        else dm_focus(w);
        return;
    }
    for(int i=order_count-1;i>=0;i--) {
        DmWindow*w=&windows[order[i]];
        if(w->minimized||!hit(w->x,w->y,w->w,w->h))continue;
        dm_focus(w);
        if(hit(w->x+w->w-42,w->y,42,30)) {
            dm_close(w);
            return;
        }
        if(hit(w->x+w->w-84,w->y,42,30)) {
            dm_maximize(w);
            return;
        }
        if(hit(w->x+w->w-126,w->y,42,30)) {
            w->minimized=1;
            return;
        }
        if(py<w->y+30&&!w->maximized) {
            drag_window=w;
            drag_x=px-w->x;
            drag_y=py-w->y;
            return;
        }
        desktop_focused=0;
        int app_x=px,app_y=py;dm_ui_untransform_window_pointer(w,&app_x,&app_y);
        if(w->app==&explorer_app&&app_x>=w->x+170&&app_y>=w->y+100&&app_y<w->y+w->h-50){Explorer*e=w->state;int row=e->offset+(app_y-w->y-100)/27;if(row>=e->count||app_x>=w->x+w->w-24){rubber.active=1;rubber.window=w;rubber.x=app_x;rubber.y=app_y;rubber.ctrl=selection_modifiers()&1;memcpy(rubber.prior,e->marks,sizeof(e->marks));if(!rubber.ctrl)memset(e->marks,0,sizeof(e->marks));e->selected=-1;return;}}
        if(w->app->click){dm_current_window=w;w->app->click(w,app_x-w->x,app_y-w->y);dm_current_window=NULL;}
        return;
    }
    for(int i=0;i<8;i++) {
        IconPosition*position=icon_position(i);
        int x=position->x,y=position->y;
        if(hit(x,y,desktop_icon_width(i),desktop_icon_height(i))) {
            begin_icon(i);
            return;
        }
    }
    for(int i=0;i<desk_count;i++) {
        IconPosition*position=icon_position(i+8);
        int x=position->x,y=position->y;
        if(hit(x,y,desktop_icon_width(i+8),desktop_icon_height(i+8))) {
            begin_icon(i+8);
            return;
        }
    }
    desktop_focused=1;rubber.active=1;rubber.window=NULL;rubber.x=px;rubber.y=py;rubber.ctrl=selection_modifiers()&1;memcpy(rubber.prior,desk_marks,sizeof(desk_marks));if(!rubber.ctrl)memset(desk_marks,0,sizeof(desk_marks));selected_icon=-1;icon_click_valid=0;
}
static int intersects(int x,int y,int w,int h,int l,int t,int r,int b){return x<r&&x+w>l&&y<b&&y+h>t;}
static void update_rubber(int held){
 if(!rubber.active)return;
 if(!held){rubber.active=0;return;}
 int current_x=px,current_y=py,start_x=rubber.x,start_y=rubber.y;if(rubber.window){dm_ui_untransform_window_pointer(rubber.window,&current_x,&current_y);start_x=rubber.x;start_y=rubber.y;}
 int l=current_x<start_x?current_x:start_x,r=current_x>start_x?current_x:start_x,t=current_y<start_y?current_y:start_y,b=current_y>start_y?current_y:start_y;
 if(rubber.window){DmWindow*w=rubber.window;if(!w->used){rubber.active=0;return;}Explorer*e=w->state;for(int i=0;i<e->count;i++){int row=i-e->offset,hit=row>=0&&w->y+100+row*27<w->y+w->h-50&&intersects(w->x+170,w->y+100+row*27,w->w-194,26,l,t,r,b);e->marks[i]=hit||(rubber.ctrl&&rubber.prior[i]);}e->selected=-1;for(int i=0;i<e->count;i++)if(e->marks[i]){e->selected=i;break;}}
 else for(int i=0;i<desk_count+8;i++){IconPosition*p=icon_position(i);desk_marks[i]=intersects(p->x,p->y,desktop_icon_width(i),desktop_icon_height(i),l,t,r,b)||(rubber.ctrl&&rubber.prior[i]);}
}
static void paper_sheet(int x,int y,int width,int height,int lean) {
    for(int row=0;row<height;row++) {
        int offset=lean*row/height;
        dm_rect(x+offset,y+row,width,1,C(242-row/2,245-row/2,249-row/2));
        dm_rect(x+offset,y+row,1,1,C(160,174,190));
        dm_rect(x+offset+width-1,y+row,1,1,C(175,185,199));
    }
    dm_line(x,y,x+width-1,y,C(165,179,195));
    for(int row=0;row<5;row++)dm_rect(x+width-5+row,y+row,5-row,1,C(188,202,220));
    dm_line(x+3,y+height/2,x+width-3,y+height/2,C(182,194,207));
}
static void draw_icon(int id,int x,int y) {
    if(!icon_atlas)return;
    const int*r=icon_regions[id];
    float box=dm_preferences.icon_size;float scale=box/(r[2]>r[3]?r[2]:r[3]);
    int draw_x=x+(box-r[2]*scale)/2,draw_y=y+(box-r[3]*scale)/2,draw_w=r[2]*scale,draw_h=r[3]*scale;dm_ui_transform_rect(&draw_x,&draw_y,&draw_w,&draw_h);vita2d_draw_texture_part_scale(icon_atlas,draw_x,draw_y,r[0],r[1],r[2],r[3],(float)draw_w/r[2],(float)draw_h/r[3]);
    if(id==4&&trash_has_items) {
        paper_sheet(x+13,y-3,12,12,4);
        paper_sheet(x+30,y-6,12,14,-3);
        paper_sheet(x+20,y-8,14,16,1);
        dm_line(x+14,y+9,x+42,y+9,C(213,224,236));
    }
}
static int icon_unit(int size,int value){int n=size*value/32;return n>0?n:1;}
static void app_icon_rect(int x,int y,int size,int px0,int py0,int w,int h,uint32_t color){dm_rect(x+icon_unit(size,px0),y+icon_unit(size,py0),icon_unit(size,w),icon_unit(size,h),color);}
static void draw_app_icon(const char*id,int x,int y,int size){
    dm_rect(x,y,size,size,C(17,43,82));
    for(int row=1;row<31;row+=5)app_icon_rect(x,y,size,1,row,30,5,(row<13)?C(89,163,225):C(46,111,179));
    app_icon_rect(x,y,size,1,1,30,4,DM_COLOR(168,221,255,190));
    app_icon_rect(x,y,size,2,30,28,1,C(13,36,68));
    if(!strcmp(id,"paint")){
        app_icon_rect(x,y,size,6,8,17,17,C(250,250,244));app_icon_rect(x,y,size,8,10,13,13,C(238,226,205));
        app_icon_rect(x,y,size,9,11,4,4,C(241,56,60));app_icon_rect(x,y,size,15,11,4,4,C(255,207,45));app_icon_rect(x,y,size,9,17,4,4,C(50,174,105));app_icon_rect(x,y,size,15,17,4,4,C(39,121,224));
        app_icon_rect(x,y,size,22,5,3,17,C(248,219,166));app_icon_rect(x,y,size,23,4,2,4,C(56,69,93));
    }else if(!strcmp(id,"calculator")){
        app_icon_rect(x,y,size,6,4,20,24,C(216,230,239));app_icon_rect(x,y,size,8,6,16,5,C(31,53,72));
        for(int r=0;r<3;r++)for(int c=0;c<3;c++)app_icon_rect(x,y,size,8+c*5,13+r*4,3,2,(r==0&&c==2)?C(244,164,58):C(66,113,156));
    }else if(!strcmp(id,"notepad")){
        app_icon_rect(x,y,size,7,4,18,24,C(247,250,253));app_icon_rect(x,y,size,8,5,16,3,C(52,126,192));
        for(int r=0;r<5;r++)app_icon_rect(x,y,size,10,11+r*3,11-(r%2)*2,1,C(82,111,137));
    }else if(!strcmp(id,"browser")){
        app_icon_rect(x,y,size,6,6,20,20,C(199,233,249));app_icon_rect(x,y,size,8,8,16,16,C(23,126,198));
        app_icon_rect(x,y,size,8,14,16,2,C(224,247,255));app_icon_rect(x,y,size,15,8,2,16,C(224,247,255));app_icon_rect(x,y,size,10,10,12,1,C(83,191,232));app_icon_rect(x,y,size,10,21,12,1,C(83,191,232));
    }else if(!strcmp(id,"media")){
        app_icon_rect(x,y,size,7,6,18,20,C(13,48,91));app_icon_rect(x,y,size,12,11,3,13,C(250,250,255));app_icon_rect(x,y,size,14,10,9,3,C(250,250,255));app_icon_rect(x,y,size,19,11,3,8,C(250,250,255));app_icon_rect(x,y,size,11,22,5,4,C(74,198,244));app_icon_rect(x,y,size,18,18,5,4,C(74,198,244));
    }else if(!strcmp(id,"pdf")){
        app_icon_rect(x,y,size,7,4,18,24,C(250,249,244));app_icon_rect(x,y,size,8,5,16,5,C(191,45,50));
        app_icon_rect(x,y,size,10,14,12,2,C(199,65,58));app_icon_rect(x,y,size,10,18,10,1,C(109,119,128));app_icon_rect(x,y,size,10,21,10,1,C(109,119,128));
    }else if(!strcmp(id,"images")){
        app_icon_rect(x,y,size,5,7,22,18,C(235,246,252));app_icon_rect(x,y,size,7,9,18,14,C(112,196,231));app_icon_rect(x,y,size,8,17,7,6,C(41,143,89));app_icon_rect(x,y,size,14,14,8,9,C(69,165,94));app_icon_rect(x,y,size,21,10,2,2,C(255,214,86));
    }else if(!strcmp(id,"console")){
        app_icon_rect(x,y,size,5,6,22,19,C(17,30,45));app_icon_rect(x,y,size,6,7,20,3,C(64,147,205));app_icon_rect(x,y,size,9,14,3,2,C(113,224,150));app_icon_rect(x,y,size,12,16,2,2,C(113,224,150));app_icon_rect(x,y,size,16,18,7,2,C(113,224,150));
    }else if(!strcmp(id,"taskmanager")){
        app_icon_rect(x,y,size,6,5,20,23,C(239,246,250));app_icon_rect(x,y,size,8,8,16,3,C(61,133,192));
        app_icon_rect(x,y,size,9,19,3,6,C(51,161,105));app_icon_rect(x,y,size,14,15,3,10,C(233,169,54));app_icon_rect(x,y,size,19,12,3,13,C(54,129,205));
    }else if(!strcmp(id,"solitaire")){
        app_icon_rect(x,y,size,6,6,15,21,C(251,252,255));app_icon_rect(x,y,size,10,4,15,22,C(255,255,255));app_icon_rect(x,y,size,13,8,8,2,C(46,93,154));app_icon_rect(x,y,size,14,13,3,5,C(203,49,56));app_icon_rect(x,y,size,19,19,3,5,C(203,49,56));
    }else if(!strcmp(id,"minesweeper")){
        app_icon_rect(x,y,size,6,5,20,22,C(210,219,226));for(int r=0;r<4;r++)for(int c=0;c<4;c++)app_icon_rect(x,y,size,8+c*4,7+r*4,3,3,((r+c)%3==0)?C(88,153,198):C(238,242,244));app_icon_rect(x,y,size,12,20,3,3,C(216,56,46));
    }else if(!strcmp(id,"explorer")){
        app_icon_rect(x,y,size,4,8,12,7,C(255,217,81));app_icon_rect(x,y,size,4,12,24,13,C(255,195,43));app_icon_rect(x,y,size,5,13,22,3,C(255,231,128));
    }else if(!strcmp(id,"network")){
        app_icon_rect(x,y,size,7,7,18,18,C(102,199,235));app_icon_rect(x,y,size,9,9,14,2,C(241,250,255));app_icon_rect(x,y,size,8,15,16,2,C(241,250,255));app_icon_rect(x,y,size,15,8,2,16,C(241,250,255));
    }else{
        unsigned hash=2166136261u;for(const unsigned char*p=(const unsigned char*)id;*p;p++)hash=(hash^*p)*16777619u;
        uint32_t tint=C(48+(hash&95),86+((hash>>8)&95),132+((hash>>16)&95));
        app_icon_rect(x,y,size,6,5,20,22,C(245,249,253));app_icon_rect(x,y,size,8,8,16,15,tint);
        char mark[3]={(char)(id[0]>='a'&&id[0]<='z'?id[0]-32:id[0]),0,0};unsigned j=1;while(id[j]&&j<sizeof(mark)-1){if((id[j]>='a'&&id[j]<='z')||(id[j]>='A'&&id[j]<='Z')){mark[1]=(char)(id[j]>='a'&&id[j]<='z'?id[j]-32:id[j]);break;}j++;}dm_text_center(x+size/2,y+icon_unit(size,19),mark,C(255,255,255));
    }
}
static int draw_app_shortcut(const char*path,int x,int y,int size){
    const char*ext=strrchr(path,'.');if(!ext||dm_ascii_casecmp(ext,".dmlink"))return 0;
    char target[DM_PATH_MAX];if(dm_fs_read(path,target,sizeof(target))<0)return 0;target[strcspn(target,"\r\n")]=0;
    if(strncmp(target,"app:",4))return 0;
    for(int i=0;i<app_count;i++)if(!strcmp(registry[i]->id,target+4)){draw_app_icon(registry[i]->id,x,y,size);return 1;}
    return 0;
}
static void draw_prompt(void) {
    dm_rect(0,0,960,544,DM_COLOR(5,12,25,180));
    dm_rect(110,105,740,365,C(234,242,250));
    dm_rect(110,105,740,30,C(40,75,130));
    dm_text(125,127,prompt.title,WHITE);
    dm_rect(130,145,700,85,WHITE);
    size_t n=strlen(prompt.text),begin=n>150?n-150:0;
    while(begin<n&&(prompt.text[begin]&0xc0)==0x80)begin++;
    char line[90];
    int row=0,col=0;size_t line_start=begin,sel_a,sel_b;prompt_selection(&sel_a,&sel_b);int caret_drawn=0;
    for(size_t i=begin;;i++) {
        char ch=prompt.text[i]&&!strncmp(prompt.title,"Password",8)?'*':prompt.text[i];
        if(!ch||ch=='\n'||col==78) {
            line[col]=0;
            if(row<3){
                int y=150+row*23;
                size_t sa=sel_a>line_start?sel_a:line_start,sb=sel_b<i?sel_b:i;
                if(sb>sa){char part[90];snprintf(part,sizeof(part),"%.*s",(int)(sb-sa),line+(sa-line_start));char prefix[90];snprintf(prefix,sizeof(prefix),"%.*s",(int)(sa-line_start),line);dm_rect(140+dm_text_width_raw(prefix),y,dm_text_width_raw(part),22,C(151,195,246));}
                dm_text_raw(140,168+row*23,line,DARK);
                if(!caret_drawn&&prompt.cursor>=line_start&&prompt.cursor<=i){char prefix[90];size_t off=prompt.cursor-line_start;if(off>(size_t)col)off=col;snprintf(prefix,sizeof(prefix),"%.*s",(int)off,line);if((dm_clock_ms()/500)&1)dm_rect(140+dm_text_width_raw(prefix),150+row*23,2,21,DARK);caret_drawn=1;}
            }
            row++;
            line_start=i;
            col=0;
            if(!ch||row>=3)break;
            if(ch=='\n'){line_start=i+1;continue;}
        }
        line[col++]=ch;
    }
    for(int row=0;row<4;row++)for(size_t col=0;col<strlen(keyboard_rows[row]);col++) {
        int x=130+(int)col*54,y=245+row*40;
        dm_rect(x,y,48,34,C(199,216,236));
        char t[2]= {
            keyboard_rows[row][col],0
        };
        if(prompt.shift&&t[0]>='a'&&t[0]<='z')t[0]-=32;
        dm_text(x+18,y+23,t,DARK);
    }
    dm_text_center(182,419,prompt.shift?"MAIUSC":"Shift",DARK);dm_text_center(355,419,"Spazio",DARK);dm_text_center(555,419,"Invio",DARK);dm_text_center(740,419,"Cancella",DARK);
    dm_rect(130,425,100,34,C(207,218,232));
    dm_text_center(180,448,"Annulla",DARK);
    dm_rect(250,425,160,34,C(207,218,232));
    dm_text_center(330,448,"Svuota testo",DARK);
    dm_rect(730,425,100,34,C(64,116,172));
    dm_text_center(780,448,"Conferma",WHITE);
}
static void draw(void) {
    vita2d_texture*t=custom?custom:walls[wall];
    if(t)vita2d_draw_texture_scale(t,0,0,960.f/vita2d_texture_get_width(t),544.f/vita2d_texture_get_height(t));
    else dm_rect(0,0,960,544,C(15,45,85));
    for(int i=0;i<8;i++) {
        IconPosition*position=icon_position(i);
        int x=position->x,y=position->y;
        if(desk_marks[i])dm_rect(x,y,desktop_icon_width(i),desktop_icon_height(i),DM_COLOR(131,191,245,95));
        draw_icon(i,x+(106-dm_preferences.icon_size)/2,y);
        icon_label(x,y+dm_preferences.icon_size+21,106,dm_ui_translate(names[i]));
    }
    dm_text(365,275,"DESKTOP MODE",DM_COLOR(215,235,255,115));
    for(int i=0;i<desk_count;i++) {
        IconPosition*position=icon_position(i+8);
        int x=position->x,y=position->y;
        if(desk_marks[i+8])dm_rect(x,y,desktop_icon_width(i+8),desktop_icon_height(i+8),DM_COLOR(131,191,245,95));
        char file_path[DM_PATH_MAX];dm_fs_join(file_path,sizeof(file_path),DESK,desk_entries[i].name);
        int icon_x=x+(100-dm_preferences.icon_size)/2;
        if(desk_entries[i].dir)draw_icon(5,icon_x,y);
        else if(!draw_app_shortcut(file_path,icon_x,y,dm_preferences.icon_size)&&!dm_file_icon(file_path,icon_x,y,dm_preferences.icon_size))draw_icon(3,icon_x,y);
        icon_label(x,y+dm_preferences.icon_size+21,100,desk_entries[i].name);
        if(strstr(desk_entries[i].name,".dmlink")){int ox=x+22,oy=y+dm_preferences.icon_size-15;dm_rect(ox,oy,17,17,WHITE);dm_rect(ox+2,oy+2,13,13,C(38,74,117));dm_line(ox+4,oy+12,ox+12,oy+4,WHITE);dm_line(ox+7,oy+4,ox+12,oy+4,WHITE);dm_line(ox+12,oy+4,ox+12,oy+9,WHITE);}
    }
    if(((pending_icon>=8&&icon_moved)||(file_drag_window&&file_drag_moved))&&over_trash()) {
        IconPosition*trash=icon_position(4);
        dm_rect(trash->x,trash->y,desktop_icon_width(4),desktop_icon_height(4),DM_COLOR(90,215,135,110));
    }
    if(desktop_drop_hover>=0&&desktop_drop_hover<desk_count) {
        IconPosition*folder=icon_position(desktop_drop_hover+8);
        dm_rect(folder->x,folder->y,desktop_icon_width(desktop_drop_hover+8),desktop_icon_height(desktop_drop_hover+8),DM_COLOR(90,170,245,135));
    }
    for(int j=0;j<order_count;j++) {
        DmWindow*w=&windows[order[j]];
        if(w->minimized)continue;
        dm_rect(w->x-3,w->y-3,w->w+6,w->h+6,DM_COLOR(14,31,55,220));
        dm_rect(w->x,w->y,w->w,w->h,C(231,239,247));
        for(int i=0;i<30;i++)dm_rect(w->x,w->y+i,w->w,1,DM_COLOR(50-i,91-i,151-i,240));
        dm_text(w->x+12,w->y+22,dm_ui_translate(w->app->title),WHITE);
        dm_text(w->x+w->w-116,w->y+22,"_",WHITE);
        dm_text(w->x+w->w-72,w->y+22,w->maximized?"<>":"[]",WHITE);
        dm_rect(w->x+w->w-42,w->y,42,30,C(171,56,56));
        dm_text(w->x+w->w-28,w->y+22,"X",WHITE);
        DmWindow*prior=dm_current_window;dm_current_window=w;w->app->draw(w);dm_current_window=prior;
    }
    if(file_drag_window&&file_drag_moved) {
        const char*name=strrchr(file_drag_path,'/');
        char label[56];
        snprintf(label,sizeof(label),"%.50s",name?name+1:file_drag_path);
        dm_rect(px+12,py+14,240,28,DM_COLOR(40,76,120,220));
        dm_text(px+18,py+34,label,WHITE);
    }
    dm_rect(0,504,960,40,DM_COLOR(19,34,65,240));
    dm_rect(4,507,start_button_width(),34,C(37,88,117));
    dm_text(18,532,"(P) START",WHITE);
    for(int i=0;i<order_count;i++) {
        DmWindow*w=&windows[order[i]];
        int slot=task_width();
        int task_x=taskbar_tasks_x()+i*slot;
        dm_rect(task_x,507,slot-2,34,top_window()==w?C(64,99,148):C(32,55,85));
        char s[128];
        snprintf(s,sizeof(s),"%.127s",dm_ui_translate(w->app->title));
        int available=slot-14,shortened=0;
        while(s[0]&&dm_text_width(s)>available) {
            size_t n=strlen(s);
            do {
                n--;
            }
            while(n&&((unsigned char)s[n]&0xc0)==0x80);
            s[n]=0;
            shortened=1;
        }
        if(shortened) {
            while(s[0]&&dm_text_width(s)+dm_text_width("...")>available) {
                size_t n=strlen(s);
                do { n--; } while(n&&((unsigned char)s[n]&0xc0)==0x80);
                s[n]=0;
            }
            if(strlen(s)+4<sizeof(s))strcat(s,"...");
        }
        dm_text(task_x+5,532,s,WHITE);
    }
    dm_clock_draw_tray();dm_device_tray_draw();dm_runtime_draw_tray();dm_network_tray_draw();
    if(start) {
        dm_rect(8,174,310,330,DM_COLOR(24,49,82,245));
        for(int i=0;i<START_ROWS&&start_offset+i<app_count;i++)dm_text(25,196+i*30,dm_ui_translate(start_app(start_offset+i)->title),WHITE);
        dm_rect(18,414,288,30,C(37,88,117));dm_text(28,437,dm_localize("Esegui...","Run...","Ejecutar..."),WHITE);
        dm_rect(18,452,135,36,C(37,88,117));dm_text(28,477,"< Precedenti",WHITE);
        dm_rect(171,452,135,36,C(37,88,117));dm_text(185,477,"Successive >",WHITE);
        char page[64];snprintf(page,sizeof(page),dm_localize("Pagina %d/%d | L/R","Page %d/%d | L/R","Pagina %d/%d | L/R"),start_offset/START_ROWS+1,(app_count+START_ROWS-1)/START_ROWS);dm_text_raw(25,501,page,WHITE);
    }
    if(menu_count) {
        dm_rect(menu_x,menu_y,300,menu_count*30,C(239,244,250));
        for(int i=0;i<menu_count;i++)dm_text(menu_x+12,menu_y+22+i*30,dm_ui_translate(menu_items[i].label),DARK);
    }
    if(rubber.active){int cx=px,cy=py,sx=rubber.x,sy=rubber.y;DmWindow*prior=dm_current_window;if(rubber.window){dm_ui_untransform_window_pointer(rubber.window,&cx,&cy);dm_current_window=rubber.window;}int x=cx<sx?cx:sx,y=cy<sy?cy:sy,w=abs(cx-sx),h=abs(cy-sy);dm_rect(x,y,w,h,DM_COLOR(90,165,240,65));dm_rect(x,y,w,1,C(100,180,250));dm_rect(x,y+h,w,1,C(100,180,250));dm_rect(x,y,1,h,C(100,180,250));dm_rect(x+w,y,1,h,C(100,180,250));dm_current_window=prior;}
    dm_clock_draw_popup();dm_device_popup_draw();
    dm_job_draw();
    dm_file_dialog_draw();
    dm_confirm_draw();dm_drop_draw();
    if(prompt.active)draw_prompt();
    for(int row=0;row<20;row++)dm_rect(px,py+row,row*3/4+1,1,DARK);
    for(int row=2;row<18;row++)dm_rect(px+2,py+row,row*3/4-1,1,WHITE);
    dm_rect(px+7,py+17,3,9,WHITE);
}
#ifdef DESKTOP_PREVIEW
#include "tests/selftest.h"
#endif
#ifdef DESKTOP_PREVIEW
static void queue_external_drop(const char*input){
    if(external_drop_count>=128){dm_status("Massimo 128 file per trascinamento");return;}
    char staged[DM_PATH_MAX],target[DM_PATH_MAX];
    if(desktop_import_dropped_file(input,staged,sizeof(staged))<0){dm_status("Impossibile importare un file dal PC");return;}
    if(!external_drop_count){
        if(!external_drop_target_at(px,py,target))snprintf(target,sizeof(target),"%s",DESK);
        snprintf(external_drop_target,sizeof(external_drop_target),"%s",target);
    }
    snprintf(external_drop_paths[external_drop_count++],DM_PATH_MAX,"%s",staged);
}
static void flush_external_drops(void){
    if(!external_drop_count)return;
    if(dm_drop_copy_begin((const char(*)[DM_PATH_MAX])external_drop_paths,external_drop_count,external_drop_target,1)<0){for(int i=0;i<external_drop_count;i++)sceIoRemove(external_drop_paths[i]);dm_status("Impossibile copiare i file nella cartella scelta");}
    external_drop_count=0;external_drop_target[0]=0;
}
#endif
static void dispatch_input(const char*input,int key){
#ifdef DESKTOP_PREVIEW
    if(key==DM_KEY_FILEDROP){
        queue_external_drop(input);
        return;
    }
#endif
    if(dm_saver_wake())return;
    if(dm_drop_active()){if(key==DM_KEY_ENTER)dm_drop_click(250,240);return;}
    if(dm_device_popup_active()){int v=dm_volume_get();if((key==DM_KEY_UP||key==DM_KEY_RIGHT)&&v>=0&&v<30)dm_volume_set(v+1);if((key==DM_KEY_DOWN||key==DM_KEY_LEFT)&&v>0)dm_volume_set(v-1);return;}
            if(prompt.active) {
                if(input[0])prompt_text(input);
                if(key)prompt_key(key);
            }     else if(dm_confirm_active()) {
                if(key==DM_KEY_ENTER)dm_confirm_click(600,330);
            }    else if(dm_file_dialog_active()) {
                if(key)dm_file_dialog_key(key);
            }    else if(dm_job_active()) {
            }     else {
                DmWindow*w=top_window();
                if(desktop_focused&&key==DM_KEY_SELECT_ALL){memset(desk_marks,1,desk_count+8);}
                else if(desktop_focused&&(key==DM_KEY_COPY||key==DM_KEY_PASTE||key==DM_KEY_DELETE)){context_window=NULL;context_target[0]=0;run_action(key==DM_KEY_COPY?ACT_COPY:key==DM_KEY_PASTE?ACT_PASTE:ACT_DELETE);}
                else if(w&&!desktop_focused) {
                    if(input[0]&&w->app->text){dm_current_window=w;w->app->text(w,input);dm_current_window=NULL;}
                    if(key&&w->app->key) {
                        dm_current_window=w;
                        if(strcmp(w->app->id,"notepad")&&strcmp(w->app->id,"console")) {
                            if(key==DM_KEY_SCROLL_UP)key=DM_KEY_UP;
                            if(key==DM_KEY_SCROLL_DOWN)key=DM_KEY_DOWN;
                        }
                        w->app->key(w,key);dm_current_window=NULL;
                    }
                }
            }
}
int main(void) {
    vita2d_init();
    vita2d_set_clear_color(C(12,25,47));
    font=vita2d_load_default_pgf();
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,SCE_TOUCH_SAMPLING_STATE_START);
    sceIoMkdir(STORE,0777);
    sceIoMkdir(DESK,0777);
    /* App modules import SceNet; load its system module before scanning them. */
#ifndef DESKTOP_PREVIEW
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
#endif
    dm_preferences_init();dm_register_app(&dm_settings_app);
    sceIoMkdir(STORE "screensavers/",0777);dm_scan_savers();
    dm_register_app(&explorer_app);
    dm_register_app(&dm_system_properties_app);
    dm_register_app(&personalize_app);
    dm_register_app(&about_app);
    dm_register_app(&dm_wasm_sandbox_app);
    extern const DmApp dm_control_panel_app;
    dm_register_app(&dm_control_panel_app);
    dm_register_app(&dm_file_types_app);
    extern const DmApp dm_choose_file_app;dm_register_app(&dm_choose_file_app);
    dm_register_app(&dm_remote_app);
    extern const DmApp dm_bluetooth_app;dm_register_app(&dm_bluetooth_app);
    dm_register_app(&dm_app_manager_app);
    dm_register_app(&dm_network_settings_app);
    sceIoMkdir(STORE "apps/",0777);
    DmUpdateResult update_result;
    dm_updates_check(&update_result);
    if(update_result.core_must_exit){
        /* Exit after promotion so the next launch maps the updated executable. */
#ifndef DESKTOP_PREVIEW
        sceKernelExitProcess(0);
#else
        return 0;
#endif
    }
    dm_scan_plugins();
    if(update_result.failed&&!update_result.apps_updated&&!update_result.core_ready)dm_status("Controllo aggiornamenti non riuscito. Consulta update.log in ux0:/data/desktop-mode/");
    else if(update_result.apps_updated&&update_result.core_ready){char message[160];snprintf(message,sizeof(message),"%d app aggiornate. Core %s scaricato ma installazione non riuscita (0x%08X).",update_result.apps_updated,update_result.tag,(unsigned)update_result.core_install_error);dm_status(message);}
    else if(update_result.apps_updated){char message[128];snprintf(message,sizeof(message),"%d app aggiornate da GitHub (%s).",update_result.apps_updated,update_result.tag);dm_status(message);}
    else if(update_result.core_ready){char message[160];snprintf(message,sizeof(message),"Core %s scaricato ma installazione non riuscita (0x%08X); VPK conservata in updates.",update_result.tag,(unsigned)update_result.core_install_error);dm_status(message);}
    dm_associations_load();
    dm_clock_initialize();
    trash_has_items=dm_trash_has_items(NULL);
    trash_checked=dm_clock_ms();
    refresh_desktop();
    load_positions();
    icon_atlas=vita2d_load_PNG_file("app0:/assets/icons-transparent.png");
    for(int i=0;i<3;i++) {
        char p[100];
        snprintf(p,sizeof(p),"app0:/assets/wallpaper%d.png",i);
        walls[i]=vita2d_load_PNG_file(p);
    }
    char config[DM_PATH_MAX+32];
    if(dm_fs_read(STORE "settings.txt",config,sizeof(config))>=0) {
        wall=atoi(config);
        if(wall<0||wall>2)wall=0;
        char*line=strchr(config,'\n');
        if(line&&line[1]) {
            snprintf(custom_path,sizeof(custom_path),"%s",line+1);
            const char*ext=strrchr(custom_path,'.');
            if(ext)custom=!dm_ascii_casecmp(ext,".png")?vita2d_load_PNG_file(custom_path):vita2d_load_JPEG_file(custom_path);
        }
    }
#ifdef DESKTOP_PREVIEW
    if(getenv("DESKTOP_SELF_TEST"))return selftest();
    if(getenv("DESKTOP_PREVIEW_SCENE")) {
        const char*scene=getenv("DESKTOP_PREVIEW_SCENE");
        if(!strcmp(scene,"trash-full")||!strcmp(scene,"trash-empty")) {
            IconPosition*p=icon_position(4);
            p->x=450;
            p->y=340;
            trash_has_items=!strcmp(scene,"trash-full");
            dm_status("");
        }         else if(!strcmp(scene,"clock"))dm_clock_click(890,525);
        else if(!strcmp(scene,"save")) {
            DmWindow*w=dm_launch("notepad",NULL);
            if(w) {
                w->app->text(w,"Un documento da salvare sul desktop o su un'unita Vita.");
                w->app->key(w,DM_KEY_SAVE);
            }
        } else if(!strcmp(scene,"notepad")||!strcmp(scene,"notepad-menu")){DmWindow*w=dm_launch("notepad",NULL);if(w&&!strcmp(scene,"notepad-menu"))w->app->click(w,20,50);}
        else if(!strcmp(scene,"control"))dm_launch("control",NULL);
        else if(!strcmp(scene,"settings"))dm_launch("settings",NULL);
        else if(!strcmp(scene,"filetypes"))dm_launch("filetypes",NULL);
        else if(!strcmp(scene,"screensaver"))dm_saver_preview();
        else if(!strcmp(scene,"files"))dm_launch("explorer",getenv("DESKTOP_PREVIEW_FILE"));
        else if(!strcmp(scene,"volume"))dm_device_tray_click(743,520);
        else if(!strcmp(scene,"bluetooth"))dm_launch("bluetooth",NULL);
        else if(!strcmp(scene,"multi")){memset(desk_marks,0,sizeof(desk_marks));desk_marks[0]=desk_marks[1]=desk_marks[3]=1;}
        else if(!strcmp(scene,"start")){start=1;start_offset=0;}
        else if(!strcmp(scene,"paint")||!strcmp(scene,"calculator")||!strcmp(scene,"taskmanager")||!strcmp(scene,"images"))dm_launch(scene,!strcmp(scene,"images")?"app0:/assets/wallpaper0.png":NULL);
        else if(!strcmp(scene,"pdf"))dm_launch("pdf",getenv("DESKTOP_PREVIEW_FILE"));
        else if(!strcmp(scene,"paint-max")){DmWindow*w=dm_launch("paint",NULL);if(w)dm_maximize(w);}
        else if(!strcmp(scene,"console"))dm_launch("console",NULL);
        else if(!strcmp(scene,"browser"))dm_launch("browser",NULL);
        else if(!strcmp(scene,"remote"))dm_launch("remote",NULL);
        else if(!strcmp(scene,"run"))show_run();
        else if(!strcmp(scene,"apps"))dm_launch("apps",NULL);
        else if(!strcmp(scene,"network"))dm_launch("network",NULL);
        else if(!strcmp(scene,"connections"))dm_launch("connections",NULL);
        else if(!strcmp(scene,"selection")) {
            DmWindow*w=dm_launch("notepad",NULL);
            if(w) {
                w->app->text(w,"Seleziona il testo con il mouse o con il joypad.\nCopia, taglia e incolla soltanto la parte selezionata.\nCtrl+A seleziona tutto; Shift + frecce estende la selezione.");
                w->app->key(w,DM_KEY_SELECT_ALL);
            }
        }         else {
            dm_launch("explorer",!strcmp(scene,"trash")?"trash:":"ux0:/data/");
            if(!strcmp(scene,"notepad"))dm_launch("notepad","ux0:/data/Documenti.txt");
        }
    }
#endif
#ifdef DESKTOP_PREVIEW
    const char*rdp_password=getenv("DESKTOP_RDP_PASSWORD");
    if(rdp_password){const char*rdp_port=getenv("DESKTOP_RDP_PORT");dm_rdp_start(rdp_password,rdp_port?(unsigned)atoi(rdp_port):3389);}
#endif
    uint64_t last_tick=dm_clock_ms();
    unsigned old=0;
    int touching=0;
#ifndef DESKTOP_PREVIEW
    int previous_hid_held=0;
#endif
    for(;;) {
        SceCtrlData pad= {
            0
        };
        sceCtrlPeekBufferPositive(0,&pad,1);
        int saver_was_active=dm_saver_active();
        unsigned pressed=pad.buttons&~old;
        old=pad.buttons;selection_pad=(pad.buttons&SCE_CTRL_SELECT)?1:0;
        int old_px=px,old_py=py;
        int ax=(int)pad.lx-128,ay=(int)pad.ly-128;
        if(abs(ax)>24)px+=ax/18;
        if(abs(ay)>24)py+=ay/18;
        if(px<0)px=0;
        if(px>959)px=959;
        if(py<0)py=0;
        if(py>543)py=543;
#ifdef DESKTOP_PREVIEW
        static int local_x=-1,local_y=-1;
        int mouse_x,mouse_y;desktop_pointer(&mouse_x,&mouse_y);
        if(mouse_x!=local_x||mouse_y!=local_y){px=mouse_x;py=mouse_y;}
        local_x=mouse_x;local_y=mouse_y;
        char input[DM_PATH_MAX];
        int key;
        while(desktop_input(input,sizeof(input),&key)) {
            dispatch_input(input,key);
        }
        flush_external_drops();
#endif
        int hid_held=0,remote_held=0;
#ifndef DESKTOP_PREVIEW
        char hid_text[8];int hid_key,hid_right;
        dm_hid_input(hid_text,sizeof(hid_text),&hid_key,&px,&py,&hid_held,&hid_right);
        if(hid_text[0]||hid_key)dispatch_input(hid_text,hid_key);
        if(hid_held&&!previous_hid_held)pressed|=SCE_CTRL_CROSS;
        if(hid_right)pressed|=SCE_CTRL_SQUARE;
        previous_hid_held=hid_held;
#endif
        dm_rdp_tick();
        static int remote_left,remote_right;
        if(!dm_rdp_connected()){
            if(remote_left){drag_window=NULL;file_drag_window=NULL;pending_icon=-1;rubber.active=0;}
            remote_left=remote_right=0;
        }
        DmRemoteEvent remote_event;
        while(dm_rdp_event(&remote_event)){
            if(remote_event.kind==1){remote_selection_modifiers=remote_event.modifiers;px=remote_event.x;py=remote_event.y;
                pointer_held=remote_event.left;
                if(remote_event.left&&!remote_left)click();
                if(remote_event.right&&!remote_right)show_context();
                remote_left=remote_event.left;remote_right=remote_event.right;
                if(drag_window&&remote_event.left){drag_window->x=px-drag_x;drag_window->y=py-drag_y;if(drag_window->x<0)drag_window->x=0;if(drag_window->x>260)drag_window->x=260;if(drag_window->y<0)drag_window->y=0;if(drag_window->y>84)drag_window->y=84;}
                dm_device_tick(px,py,remote_event.left);update_rubber(remote_event.left);update_icon_drag(remote_event.left);update_file_drag(remote_event.left);update_start_drag(remote_event.left);
                if(remote_event.key)dispatch_input("",remote_event.key);
                remote_selection_modifiers=-1;
            }else dispatch_input(remote_event.text,remote_event.key);
        }
        remote_held=remote_left;
        SceTouchData touch= {
            0
        };
        sceTouchPeek(SCE_TOUCH_PORT_FRONT,&touch,1);
        if(touch.reportNum) {
            px=touch.report[0].x/2;
            py=touch.report[0].y/2;
            if(!touching)click();
        }
        if(drag_window&&(touch.reportNum||(pad.buttons&SCE_CTRL_CROSS)||hid_held||remote_held)) {
            drag_window->x=px-drag_x;
            drag_window->y=py-drag_y;
            if(drag_window->x<0)drag_window->x=0;
            if(drag_window->x>260)drag_window->x=260;
            if(drag_window->y<0)drag_window->y=0;
            if(drag_window->y>84)drag_window->y=84;
        }      else drag_window=NULL;
        touching=touch.reportNum>0;
        pointer_held=touch.reportNum||(pad.buttons&SCE_CTRL_CROSS)||hid_held||remote_held;
        if(pressed&SCE_CTRL_CROSS)click();
        dm_device_tick(px,py,pointer_held);
        update_rubber(pointer_held);
        update_icon_drag(pointer_held);
        update_file_drag(pointer_held);
        update_start_drag(pointer_held);
        unsigned saver_activity=pad.buttons|pressed;
        if(saver_was_active)pressed=0;
        if(pressed&SCE_CTRL_START&&!prompt.active&&!dm_file_dialog_active()&&!dm_confirm_active()&&!dm_job_active())start=!start;
        if(pressed&SCE_CTRL_SQUARE&&!prompt.active&&!dm_file_dialog_active()&&!dm_confirm_active()&&!dm_job_active()) {
            if(menu_count)menu_count=0;
            else show_context();
        }
        if(pressed&SCE_CTRL_CIRCLE) {
            if(dm_drop_active()){dm_drop_cancel();continue;}
            if(prompt.active)prompt_done(0);
            else if(dm_confirm_active())dm_confirm_cancel();
            else if(dm_file_dialog_active())dm_file_dialog_cancel();
            else if(dm_job_active())dm_job_cancel();
            else if(dm_device_popup_active())dm_device_dismiss();
            else if(dm_clock_popup_active())dm_clock_dismiss();
            else if(menu_count||start)menu_count=start=0;
            else {
                DmWindow*w=top_window();
                if(w&&w->app==&explorer_app) {
                    Explorer*e=w->state;
                    dm_fs_parent(e->path);
                    explorer_scan(w);
                }      else if(w)dm_close(w);
            }
        }
        if(start) {
            if(pressed&SCE_CTRL_RTRIGGER)start_page(1);
            if(pressed&SCE_CTRL_LTRIGGER)start_page(-1);
        }
        dm_job_tick();
        poll_mounts(dm_clock_ms());
        if(mount_dialog_dirty&&!prompt.active&&!dm_confirm_active()&&!dm_job_active()){dm_file_dialog_refresh();mount_dialog_dirty=0;}
        if(fs_dirty&&pending_icon<0&&!dm_job_active()&&!dm_file_dialog_active()&&!prompt.active&&!dm_confirm_active())refresh_all();
        if(dm_file_dialog_active()&&!prompt.active&&!dm_confirm_active()) {
            if(pressed&SCE_CTRL_RTRIGGER)dm_file_dialog_key(DM_KEY_DOWN);
            if(pressed&SCE_CTRL_LTRIGGER)dm_file_dialog_key(DM_KEY_UP);
        }
        uint64_t now=dm_clock_ms();
        if(now-trash_checked>=2000) {
            trash_has_items=dm_trash_has_items(NULL);
            trash_checked=now;
        }
        unsigned elapsed=(unsigned)(now-last_tick);
        last_tick=now;
        if(elapsed>250)elapsed=250;
        for(int i=0;i<DM_MAX_WINDOWS;i++)if(windows[i].used&&windows[i].app->tick){dm_current_window=&windows[i];windows[i].app->tick(&windows[i],elapsed);dm_current_window=NULL;}
        DmWindow*w=top_window();
        if(w&&w->app->key&&!prompt.active&&!start&&!dm_file_dialog_active()&&!dm_confirm_active()&&!dm_job_active()) {
            if(pressed&SCE_CTRL_RTRIGGER)w->app->key(w,!strcmp(w->app->id,"notepad")?DM_KEY_SCROLL_DOWN:DM_KEY_DOWN);
            if(pressed&SCE_CTRL_LTRIGGER)w->app->key(w,!strcmp(w->app->id,"notepad")?DM_KEY_SCROLL_UP:DM_KEY_UP);
        }
        dm_saver_tick(elapsed,saver_activity||touch.reportNum||hid_held||remote_held||px!=old_px||py!=old_py,prompt.active||dm_confirm_active()||dm_file_dialog_active()||dm_job_active()||dm_drop_active());
        if(file_icons_dirty&&!dm_job_active()){
#ifndef DESKTOP_PREVIEW
            vita2d_wait_rendering_done();
#endif
            dm_file_icons_clear();file_icons_dirty=0;
        }
        vita2d_start_drawing();
        vita2d_clear_screen();
        if(dm_saver_active())dm_saver_draw();else draw();
        vita2d_end_drawing();
#ifdef DESKTOP_PREVIEW
        dm_rdp_capture();
#endif
        vita2d_swap_buffers();
#ifndef DESKTOP_PREVIEW
        dm_rdp_capture();
#endif
        sceKernelDelayThread(16000);
    }
    return 0;
}
