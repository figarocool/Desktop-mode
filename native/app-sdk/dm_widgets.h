#ifndef DM_WIDGETS_H
#define DM_WIDGETS_H

/* Immediate-mode Windows 95-style controls shared by native and WASM apps.
 * Widgets own no global state: keep values/selections in the app and call the
 * *_click helpers from the app's click callback. Coordinates use the app's
 * normal coordinate system. */
#include <stdint.h>

#if defined(DM_WIDGETS_WASM)
#include "wasm_app.h"
#define DMW_RECT dm_host_rect
#define DMW_TEXT dm_host_text
#define DMW_WIDTH dm_host_text_width
#define DMW_SCROLL dm_host_scrollbar_draw
#elif defined(DM_WIDGETS_NATIVE)
#include "../desktop_api.h"
#define DMW_RECT dm_rect
#define DMW_TEXT dm_text_raw
#define DMW_WIDTH dm_text_width_raw
#define DMW_SCROLL dm_scrollbar_draw
#else
#include "desktop_plugin.h"
#define DMW_RECT dm_rect
#define DMW_TEXT dm_text_raw
#define DMW_WIDTH dm_text_width_raw
#define DMW_SCROLL dm_scrollbar_draw
#endif

enum {
    DMW_DISABLED=1u<<0, DMW_DEFAULT=1u<<1, DMW_READONLY=1u<<2,
    DMW_PASSWORD=1u<<3, DMW_MULTILINE=1u<<4,
    DMW_MULTISELECT=1u<<5, DMW_VERTICAL=1u<<6, DMW_FOCUSED=1u<<7
};
enum {DMW_TREE_HAS_CHILDREN=1u<<0,DMW_TREE_EXPANDED=1u<<1};
typedef struct {int x,y,w,h;} DmWidgetRect;
typedef struct {const char *text;int width;} DmWidgetColumn;
typedef struct {const char *text;unsigned depth;unsigned flags;} DmWidgetTreeNode;

static inline int dmw_contains(DmWidgetRect r,int x,int y){return x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h;}
static inline uint32_t dmw_color(unsigned r,unsigned g,unsigned b){
#if defined(DM_WIDGETS_WASM)
    return 0xFF000000u|(r<<16)|(g<<8)|b;
#else
    return DM_COLOR(r,g,b,255);
#endif
}
static inline void dmw_border(DmWidgetRect r,int sunken){
    uint32_t hi=sunken?dmw_color(96,96,96):dmw_color(255,255,255),lo=sunken?dmw_color(255,255,255):dmw_color(96,96,96);
    DMW_RECT(r.x,r.y,r.w,1,hi);DMW_RECT(r.x,r.y,1,r.h,hi);DMW_RECT(r.x,r.y+r.h-1,r.w,1,lo);DMW_RECT(r.x+r.w-1,r.y,1,r.h,lo);
}
static inline void dmw_label(DmWidgetRect r,const char *text,uint32_t color){DMW_TEXT(r.x,r.y+r.h-5,text?text:"",color);}
static inline void dmw_button(DmWidgetRect r,const char *text,unsigned flags,int pressed){
    DMW_RECT(r.x,r.y,r.w,r.h,(flags&DMW_DISABLED)?dmw_color(192,192,192):dmw_color(192,192,192));dmw_border(r,pressed);
    if(flags&DMW_DEFAULT){DMW_RECT(r.x+2,r.y+2,r.w-4,1,dmw_color(0,0,0));DMW_RECT(r.x+2,r.y+r.h-3,r.w-4,1,dmw_color(0,0,0));}
    int tw=DMW_WIDTH(text?text:"");int tx=r.x+(r.w-tw)/2+(pressed?1:0);if(tx<r.x+4)tx=r.x+4;
    DMW_TEXT(tx,r.y+r.h/2+5+(pressed?1:0),text?text:"",(flags&DMW_DISABLED)?dmw_color(128,128,128):dmw_color(0,0,0));
}
static inline int dmw_button_click(DmWidgetRect r,int x,int y,unsigned flags){return !(flags&DMW_DISABLED)&&dmw_contains(r,x,y);}
enum {DMW_ICON_PLAY=1,DMW_ICON_PAUSE,DMW_ICON_STOP,DMW_ICON_PREVIOUS,DMW_ICON_NEXT,DMW_ICON_REWIND,DMW_ICON_FORWARD,DMW_ICON_SPEAKER,DMW_ICON_MUTE,DMW_ICON_PLAYLIST};
static inline void dmw_icon_line(int x0,int y0,int x1,int y1,uint32_t color){int dx=x1>x0?x1-x0:x0-x1,sx=x0<x1?1:-1,dy=y1>y0?y0-y1:y1-y0,sy=y0<y1?1:-1,err=dx+dy;for(;;){DMW_RECT(x0,y0,1,1,color);if(x0==x1&&y0==y1)break;int e=2*err;if(e>=dy){err+=dy;x0+=sx;}if(e<=dx){err+=dx;y0+=sy;}}}
static inline void dmw_icon_triangle(int cx,int cy,int right,int half_width,int half_height,uint32_t color){for(int y=-half_height;y<=half_height;y++){int span=half_width*(half_height-(y<0?-y:y))/half_height;if(right)DMW_RECT(cx-half_width,cy+y,span+1,1,color);else DMW_RECT(cx+half_width-span,cy+y,span+1,1,color);}}
static inline void dmw_icon_button(DmWidgetRect r,int icon,unsigned flags,int pressed){
    DMW_RECT(r.x,r.y,r.w,r.h,dmw_color(192,192,192));dmw_border(r,pressed);
    uint32_t ink=(flags&DMW_DISABLED)?dmw_color(128,128,128):dmw_color(20,25,30);int cx=r.x+r.w/2,cy=r.y+r.h/2;
    if(icon==DMW_ICON_PLAY)dmw_icon_triangle(cx,cy,1,6,7,ink);
    else if(icon==DMW_ICON_PAUSE){DMW_RECT(cx-5,cy-6,3,12,ink);DMW_RECT(cx+2,cy-6,3,12,ink);}
    else if(icon==DMW_ICON_STOP)DMW_RECT(cx-5,cy-5,10,10,ink);
    else if(icon==DMW_ICON_PREVIOUS){DMW_RECT(cx+6,cy-6,1,12,ink);dmw_icon_triangle(cx,cy,0,6,7,ink);}
    else if(icon==DMW_ICON_NEXT){DMW_RECT(cx-6,cy-6,1,12,ink);dmw_icon_triangle(cx,cy,1,6,7,ink);}
    else if(icon==DMW_ICON_REWIND){dmw_icon_triangle(cx-3,cy,0,4,5,ink);dmw_icon_triangle(cx+4,cy,0,4,5,ink);}
    else if(icon==DMW_ICON_FORWARD){dmw_icon_triangle(cx-4,cy,1,4,5,ink);dmw_icon_triangle(cx+3,cy,1,4,5,ink);}
    else if(icon==DMW_ICON_SPEAKER||icon==DMW_ICON_MUTE){DMW_RECT(cx-8,cy-3,3,6,ink);for(int y=-6;y<=6;y++){int width=1+(6-(y<0?-y:y))*3/6;DMW_RECT(cx-4,cy+y,width,1,ink);}if(icon==DMW_ICON_MUTE){dmw_icon_line(cx+4,cy-3,cx+8,cy+3,ink);dmw_icon_line(cx+8,cy-3,cx+4,cy+3,ink);}else{DMW_RECT(cx+4,cy-4,1,1,ink);DMW_RECT(cx+5,cy-3,1,1,ink);DMW_RECT(cx+6,cy-2,1,1,ink);DMW_RECT(cx+7,cy-1,1,3,ink);DMW_RECT(cx+6,cy+2,1,1,ink);DMW_RECT(cx+5,cy+3,1,1,ink);DMW_RECT(cx+4,cy+4,1,1,ink);}}
    else if(icon==DMW_ICON_PLAYLIST){for(int i=0;i<3;i++){DMW_RECT(cx-8,cy-6+i*6,11,2,ink);DMW_RECT(cx+5,cy-6+i*6,2,2,ink);}}
}
static inline int dmw_icon_button_click(DmWidgetRect r,int x,int y,unsigned flags){return dmw_button_click(r,x,y,flags);}
static inline void dmw_checkbox(DmWidgetRect r,const char *text,int checked,unsigned flags){DmWidgetRect b={r.x,r.y+(r.h-13)/2,13,13};DMW_RECT(b.x,b.y,b.w,b.h,dmw_color(255,255,255));dmw_border(b,1);if(checked){DMW_TEXT(b.x+2,b.y+11,"x",dmw_color(0,0,0));}dmw_label((DmWidgetRect){r.x+19,r.y,r.w-19,r.h},text,(flags&DMW_DISABLED)?dmw_color(128,128,128):dmw_color(0,0,0));}
static inline int dmw_checkbox_click(DmWidgetRect r,int x,int y,unsigned flags,int *checked){if((flags&DMW_DISABLED)||!checked||!dmw_contains(r,x,y))return 0;*checked=!*checked;return 1;}
static inline void dmw_radio(DmWidgetRect r,const char *text,int checked,unsigned flags){DmWidgetRect b={r.x,r.y+(r.h-13)/2,13,13};DMW_RECT(b.x,b.y,b.w,b.h,dmw_color(255,255,255));dmw_border(b,1);if(checked)DMW_TEXT(b.x+2,b.y+11,"*",dmw_color(0,0,0));dmw_label((DmWidgetRect){r.x+19,r.y,r.w-19,r.h},text,(flags&DMW_DISABLED)?dmw_color(128,128,128):dmw_color(0,0,0));}
static inline int dmw_radio_click(DmWidgetRect r,int x,int y,unsigned flags,int *selected,int value){if((flags&DMW_DISABLED)||!selected||!dmw_contains(r,x,y))return 0;*selected=value;return 1;}
static inline void dmw_groupbox(DmWidgetRect r,const char *title){int tw=DMW_WIDTH(title?title:"");DMW_RECT(r.x,r.y+6,r.w,1,dmw_color(128,128,128));DMW_RECT(r.x,r.y+7,1,r.h-7,dmw_color(128,128,128));DMW_RECT(r.x+r.w-1,r.y+7,1,r.h-7,dmw_color(255,255,255));DMW_RECT(r.x,r.y+r.h-1,r.w,1,dmw_color(255,255,255));DMW_RECT(r.x+8,r.y,tw+8,12,dmw_color(192,192,192));DMW_TEXT(r.x+12,r.y+10,title?title:"",dmw_color(0,0,0));}
static inline void dmw_edit(DmWidgetRect r,const char *text,unsigned flags,int cursor){DMW_RECT(r.x,r.y,r.w,r.h,(flags&DMW_DISABLED)?dmw_color(212,208,200):dmw_color(255,255,255));dmw_border(r,1);if(text){uint32_t fg=(flags&DMW_DISABLED)?dmw_color(128,128,128):dmw_color(0,0,0);if(flags&DMW_MULTILINE){int x=r.x+5,y=r.y+15;for(int i=0;text[i]&&y<r.y+r.h-3;i++){if(text[i]=='\n'){x=r.x+5;y+=18;continue;}char glyph[2]={text[i],0};if(flags&DMW_PASSWORD)glyph[0]='*';DMW_TEXT(x,y,glyph,fg);x+=8;}}else if(flags&DMW_PASSWORD){for(int i=0;text[i]&&i<128;i++)DMW_TEXT(r.x+5+i*8,r.y+r.h/2+5,"*",fg);}else DMW_TEXT(r.x+5,r.y+r.h/2+5,text,fg);}if((flags&DMW_FOCUSED)&&cursor>=0&&!(flags&DMW_MULTILINE)){int cx=r.x+5+cursor*8;DMW_RECT(cx,r.y+4,1,r.h-8,dmw_color(0,0,0));}}
static inline int dmw_edit_click(DmWidgetRect r,int x,int y,unsigned flags){return !(flags&DMW_DISABLED)&&dmw_contains(r,x,y);}
static inline int dmw_scrollbar_click(DmWidgetRect r,int x,int y,int content,int viewport,int *offset);
static inline void dmw_listbox_metrics(DmWidgetRect r,const char *const *items,int count,int *rows_out,int *vbar_out,int *hbar_out,int *content_width_out){int width=0;for(int i=0;i<count;i++){int n=DMW_WIDTH(items&&items[i]?items[i]:"");if(n>width)width=n;}int rows=(r.h-4)/20;if(rows<1)rows=1;int vb=count>rows,hb=0;for(int pass=0;pass<3;pass++){hb=width>r.w-12-(vb?12:0);rows=(r.h-4-(hb?12:0))/20;if(rows<1)rows=1;vb=count>rows;}if(rows_out)*rows_out=rows;if(vbar_out)*vbar_out=vb;if(hbar_out)*hbar_out=hb;if(content_width_out)*content_width_out=width;}
static inline void dmw_text_clip(int x,int baseline,const char *text,int offset,int width,uint32_t color){if(!text||width<=0)return;char slice[512],prefix[512];int length=0;while(text[length]&&length<(int)sizeof(slice)-1)length++;int start=0;for(int end=1;end<=length;end++){if(end<length&&(((unsigned char)text[end]&0xC0)==0x80))continue;for(int i=0;i<end;i++)prefix[i]=text[i];prefix[end]=0;if(DMW_WIDTH(prefix)>offset)break;start=end;}int out=0;for(int end=start+1;end<=length&&out<(int)sizeof(slice)-1;end++){if(end<length&&(((unsigned char)text[end]&0xC0)==0x80))continue;int n=end-start;for(int i=0;i<n;i++)slice[i]=text[start+i];slice[n]=0;if(DMW_WIDTH(slice)>width)break;out=n;}slice[out]=0;DMW_TEXT(x,baseline,slice,color);}
static inline void dmw_listbox_ex(DmWidgetRect r,const char *const *items,int count,uint32_t selected,int first_row,int horizontal_offset,unsigned flags){int rows=0,vbar=0,hbar=0,max_width=0;dmw_listbox_metrics(r,items,count,&rows,&vbar,&hbar,&max_width);int inner_w=r.w-4-(vbar?12:0),inner_h=r.h-4-(hbar?12:0);if(inner_w<1||inner_h<1)return;DMW_RECT(r.x,r.y,r.w,r.h,dmw_color(255,255,255));dmw_border(r,1);if(first_row<0)first_row=0;if(first_row>count-rows)first_row=count-rows;if(first_row<0)first_row=0;for(int row=0;row<rows&&first_row+row<count;row++){int i=first_row+row,yy=r.y+2+row*20,active=(flags&DMW_MULTISELECT)?(i<32&&(selected&(1u<<(unsigned)i))):(i==(int)selected);if(active)DMW_RECT(r.x+2,yy,inner_w,20,dmw_color(0,0,128));dmw_text_clip(r.x+6,yy+15,items&&items[i]?items[i]:"",horizontal_offset,inner_w-8,active?dmw_color(255,255,255):dmw_color(0,0,0));}if(vbar)DMW_SCROLL(r.x+r.w-12,r.y+2,r.h-(hbar?14:4),10,1,count,rows,first_row);if(hbar)DMW_SCROLL(r.x+2,r.y+r.h-12,r.w-(vbar?14:4),10,0,max_width,inner_w-8,horizontal_offset);}
static inline void dmw_listbox(DmWidgetRect r,const char *const *items,int count,uint32_t selected,int first,unsigned flags){dmw_listbox_ex(r,items,count,selected,first,0,flags);}
static inline int dmw_listbox_click_ex(DmWidgetRect r,int x,int y,int count,int first,int rows,int has_vbar,int has_hbar,unsigned flags){if((flags&DMW_DISABLED)||!dmw_contains(r,x,y)||x<r.x+2||x>=r.x+r.w-(has_vbar?12:2)||y<r.y+2||y>=r.y+r.h-(has_hbar?12:2))return -1;int row=(y-r.y-2)/20,index=first+row;return row>=rows||index>=count?-1:index;}
static inline int dmw_listbox_click(DmWidgetRect r,int x,int y,int count,int first,unsigned flags){int rows=(r.h-4)/20;if(rows<1)rows=1;return dmw_listbox_click_ex(r,x,y,count,first,rows,count>rows,0,flags);}
static inline int dmw_listbox_scroll_click(DmWidgetRect r,int x,int y,const char *const *items,int count,int *first_row,int *horizontal_offset){int rows=0,vbar=0,hbar=0,width=0;dmw_listbox_metrics(r,items,count,&rows,&vbar,&hbar,&width);if(vbar&&first_row&&dmw_scrollbar_click((DmWidgetRect){r.x+r.w-12,r.y+2,10,r.h-(hbar?14:4)},x,y,count,rows,first_row))return 1;int viewport=r.w-4-(vbar?12:0)-8;if(hbar&&horizontal_offset&&dmw_scrollbar_click((DmWidgetRect){r.x+2,r.y+r.h-12,r.w-(vbar?14:4),10},x,y,width,viewport,horizontal_offset))return 1;return 0;}
static inline uint32_t dmw_listbox_select(uint32_t selection,int index,unsigned flags){if(index<0||index>=32)return selection;uint32_t bit=1u<<(unsigned)index;return(flags&DMW_MULTISELECT)?selection^bit:bit;}
static inline void dmw_combo(DmWidgetRect r,const char *selected,unsigned flags,int open){DMW_RECT(r.x,r.y,r.w,r.h,dmw_color(255,255,255));dmw_border(r,1);DMW_TEXT(r.x+5,r.y+r.h/2+5,selected?selected:"",dmw_color(0,0,0));DmWidgetRect a={r.x+r.w-20,r.y,20,r.h};dmw_button(a,"v",flags,open);}
static inline int dmw_combo_click(DmWidgetRect r,int x,int y,unsigned flags){return !(flags&DMW_DISABLED)&&dmw_contains(r,x,y);}
/* Dropdown combo: returns -2 to toggle the popup, an item index to select, -1 otherwise. */
static inline void dmw_combo_ex(DmWidgetRect r,const char *const *items,int count,int selected,int first_row,int horizontal_offset,int popup_rows,unsigned flags,int open){const char*value=selected>=0&&selected<count?items[selected]:"";dmw_combo(r,value,flags,open);if(!open||popup_rows<1)return;DmWidgetRect popup={r.x,r.y+r.h,r.w,popup_rows*20+16};dmw_listbox_ex(popup,items,count,(uint32_t)selected,first_row,horizontal_offset,flags);}
static inline int dmw_combo_ex_click(DmWidgetRect r,int x,int y,const char *const *items,int count,int first_row,int popup_rows,int open,unsigned flags){if(flags&DMW_DISABLED)return -1;if(dmw_combo_click(r,x,y,flags))return -2;if(!open||popup_rows<1)return -1;DmWidgetRect popup={r.x,r.y+r.h,r.w,popup_rows*20+16};int rows=0,vbar=0,hbar=0;dmw_listbox_metrics(popup,items,count,&rows,&vbar,&hbar,0);return dmw_listbox_click_ex(popup,x,y,count,first_row,rows,vbar,hbar,flags);}
static inline void dmw_progress(DmWidgetRect r,int value,int minimum,int maximum,unsigned flags){DMW_RECT(r.x,r.y,r.w,r.h,dmw_color(255,255,255));dmw_border(r,1);if(maximum<=minimum)return;if(value<minimum)value=minimum;if(value>maximum)value=maximum;int vertical=(flags&DMW_VERTICAL)!=0,inner=vertical?r.h-4:r.w-4,fill=(int)((int64_t)inner*(value-minimum)/(maximum-minimum));for(int p=0;p<fill;p+=10){int size=(fill-p)<8?(fill-p):8;if(vertical)DMW_RECT(r.x+2,r.y+r.h-2-p-size,r.w-4,size,dmw_color(0,0,128));else DMW_RECT(r.x+2+p,r.y+2,size,r.h-4,dmw_color(0,0,128));}}
static inline void dmw_trackbar(DmWidgetRect r,int value,int minimum,int maximum,unsigned flags){int vertical=(flags&DMW_VERTICAL)!=0,travel=(vertical?r.h:r.w)-12;if(travel<0)travel=0;if(vertical)DMW_RECT(r.x+r.w/2-2,r.y,r.w>4?4:r.w,r.h,dmw_color(128,128,128));else DMW_RECT(r.x,r.y+r.h/2-2,r.w,4,dmw_color(128,128,128));if(maximum<=minimum)return;if(value<minimum)value=minimum;if(value>maximum)value=maximum;int pos=(int)((int64_t)travel*(value-minimum)/(maximum-minimum));DmWidgetRect thumb=vertical?(DmWidgetRect){r.x+(r.w-14)/2,r.y+r.h-12-pos,14,12}:(DmWidgetRect){r.x+pos,r.y+(r.h-14)/2,12,14};DMW_RECT(thumb.x,thumb.y,thumb.w,thumb.h,dmw_color(192,192,192));dmw_border(thumb,0);}
static inline int dmw_trackbar_click(DmWidgetRect r,int x,int y,int minimum,int maximum,unsigned flags,int *value){int vertical=(flags&DMW_VERTICAL)!=0,travel=(vertical?r.h:r.w)-12;if((flags&DMW_DISABLED)||!value||maximum<=minimum||travel<=0||!dmw_contains(r,x,y))return 0;int pos=vertical?r.y+r.h-6-y:x-r.x-6;if(pos<0)pos=0;if(pos>travel)pos=travel;*value=minimum+(int)((int64_t)pos*(maximum-minimum)/travel);return 1;}
static inline void dmw_tabs(DmWidgetRect r,const char *const *labels,int count,int selected){int x=r.x;for(int i=0;i<count;i++){int w=DMW_WIDTH(labels[i])+22;if(w<48)w=48;DmWidgetRect tab={x,r.y+(i==selected?0:3),w,r.h-(i==selected?0:3)};DMW_RECT(tab.x,tab.y,tab.w,tab.h,dmw_color(192,192,192));dmw_border(tab,0);DMW_TEXT(tab.x+10,tab.y+tab.h/2+5,labels[i],dmw_color(0,0,0));x+=w-1;}}
static inline int dmw_tabs_click(DmWidgetRect r,const char *const *labels,int count,int x,int y){if(!dmw_contains(r,x,y))return -1;int at=r.x;for(int i=0;i<count;i++){int w=DMW_WIDTH(labels[i])+22;if(w<48)w=48;if(x<at+w)return i;at+=w-1;}return -1;}
/* Classic menu bar and popup menu. Item state is owned by the application. */
static inline int dmw_menu_item_width(const char *label){int w=DMW_WIDTH(label?label:"")+18;return w<36?36:w;}
static inline void dmw_menubar(DmWidgetRect r,const char *const *labels,int count,int selected){DMW_RECT(r.x,r.y,r.w,r.h,dmw_color(192,192,192));int x=r.x+2;for(int i=0;i<count&&x<r.x+r.w;i++){int w=dmw_menu_item_width(labels[i]);if(x+w>r.x+r.w)w=r.x+r.w-x;if(i==selected)dmw_border((DmWidgetRect){x,r.y+1,w,r.h-2},1);DMW_TEXT(x+9,r.y+r.h/2+5,labels[i]?labels[i]:"",dmw_color(0,0,0));x+=w;}}
static inline int dmw_menubar_click(DmWidgetRect r,const char *const *labels,int count,int x,int y){if(!dmw_contains(r,x,y))return -1;int at=r.x+2;for(int i=0;i<count;i++){int w=dmw_menu_item_width(labels[i]);if(at+w>r.x+r.w)w=r.x+r.w-at;if(x>=at&&x<at+w)return i;at+=w;if(at>=r.x+r.w)break;}return -1;}
static inline void dmw_menu_popup(DmWidgetRect r,const char *const *labels,int count,int selected,int disabled_mask){DMW_RECT(r.x,r.y,r.w,r.h,dmw_color(192,192,192));dmw_border(r,0);int rows=r.h/22;if(rows>count)rows=count;for(int i=0;i<rows;i++){int y=r.y+i*22,disabled=i<32&&(disabled_mask&(1u<<(unsigned)i));if(i==selected)DMW_RECT(r.x+2,y+1,r.w-4,20,dmw_color(0,0,128));DMW_TEXT(r.x+9,y+15,labels[i]?labels[i]:"",disabled?dmw_color(128,128,128):i==selected?dmw_color(255,255,255):dmw_color(0,0,0));}}
static inline int dmw_menu_popup_click(DmWidgetRect r,int x,int y,int count,int disabled_mask){if(!dmw_contains(r,x,y)||x<r.x+2||x>=r.x+r.w-2)return -1;int i=(y-r.y)/22;return i>=0&&i<count&&(i>=32||!(disabled_mask&(1u<<(unsigned)i)))?i:-1;}
static inline void dmw_header(DmWidgetRect r,const DmWidgetColumn *columns,int count){int x=r.x;DMW_RECT(r.x,r.y,r.w,r.h,dmw_color(192,192,192));for(int i=0;i<count&&x<r.x+r.w;i++){int w=columns[i].width;if(w<1)continue;DmWidgetRect cell={x,r.y,w,r.h};dmw_border(cell,0);DMW_TEXT(x+5,r.y+r.h/2+5,columns[i].text,dmw_color(0,0,0));x+=w;}}
static inline void dmw_listview_row(DmWidgetRect r,const DmWidgetColumn *columns,const char *const *cells,int count,int selected,int row){uint32_t bg=selected?dmw_color(0,0,128):(row&1)?dmw_color(255,255,255):dmw_color(240,240,240),fg=selected?dmw_color(255,255,255):dmw_color(0,0,0);DMW_RECT(r.x,r.y,r.w,r.h,bg);int x=r.x;for(int i=0;i<count&&x<r.x+r.w;i++){int width=columns[i].width;if(width<1)continue;DMW_TEXT(x+5,r.y+r.h/2+5,cells&&cells[i]?cells[i]:"",fg);x+=width;}}
static inline void dmw_statusbar(DmWidgetRect r,const char *const *parts,int count){int x=r.x;DMW_RECT(r.x,r.y,r.w,r.h,dmw_color(192,192,192));for(int i=0;i<count&&x<r.x+r.w;i++){int w=DMW_WIDTH(parts[i])+18;if(w<50)w=50;if(x+w>r.x+r.w)w=r.x+r.w-x;DmWidgetRect cell={x,r.y,w,r.h};dmw_border(cell,1);DMW_TEXT(x+6,r.y+r.h/2+5,parts[i],dmw_color(0,0,0));x+=w;}}
static inline void dmw_toolbar_button(DmWidgetRect r,const char *label,unsigned flags,int hot){dmw_button(r,label,flags,hot);}
static inline int dmw_scrollbar_click(DmWidgetRect r,int x,int y,int content,int viewport,int *offset){if(!offset||content<=viewport||viewport<=0||!dmw_contains(r,x,y))return 0;int vertical=r.h>=r.w,length=vertical?r.h:r.w,pos=vertical?y-r.y:x-r.x;if(length<=0)return 0;int thumb=length*viewport/content;if(thumb<12)thumb=12;if(thumb>length)thumb=length;int travel=length-thumb;if(travel<=0)return 0;pos-=thumb/2;if(pos<0)pos=0;if(pos>travel)pos=travel;*offset=(content-viewport)*pos/travel;return 1;}
static inline int dmw_treeview_row_height(void){int height=DMW_WIDTH("M")*2;if(height<20)height=20;if(height>56)height=56;return height;}
static inline void dmw_treeview_metrics(DmWidgetRect r,const DmWidgetTreeNode *nodes,int count,int *rows_out,int *vbar_out,int *hbar_out,int *content_width_out){int width=0,rh=dmw_treeview_row_height();for(int i=0;i<count;i++){int n=(int)nodes[i].depth*16+DMW_WIDTH(nodes[i].text?nodes[i].text:"")+24;if(n>width)width=n;}int rows=(r.h-4)/rh;if(rows<1)rows=1;int vb=count>rows,hb=0;for(int pass=0;pass<3;pass++){hb=width>r.w-12-(vb?12:0);rows=(r.h-4-(hb?12:0))/rh;if(rows<1)rows=1;vb=count>rows;}if(rows_out)*rows_out=rows;if(vbar_out)*vbar_out=vb;if(hbar_out)*hbar_out=hb;if(content_width_out)*content_width_out=width;}
static inline void dmw_treeview(DmWidgetRect r,const DmWidgetTreeNode *nodes,int count,int selected,int first_row,int horizontal_offset){int rows=0,vbar=0,hbar=0,max_width=0,rh=dmw_treeview_row_height();dmw_treeview_metrics(r,nodes,count,&rows,&vbar,&hbar,&max_width);int inner_w=r.w-4-(vbar?12:0),inner_h=r.h-4-(hbar?12:0);if(inner_w<1||inner_h<1)return;DMW_RECT(r.x,r.y,r.w,r.h,dmw_color(255,255,255));dmw_border(r,1);if(first_row<0)first_row=0;if(first_row>count-rows)first_row=count-rows;if(first_row<0)first_row=0;for(int row=0;row<rows&&first_row+row<count;row++){int i=first_row+row,y=r.y+2+row*rh,selected_row=i==selected,content_x=5+(int)nodes[i].depth*16,text_content_x=content_x+14,text_screen_x=r.x+text_content_x-horizontal_offset,clip_x=r.x+6,draw_x=text_screen_x<clip_x?clip_x:text_screen_x,crop=text_screen_x<clip_x?clip_x-text_screen_x:0,available=r.x+inner_w-2-draw_x;if(selected_row)DMW_RECT(r.x+2,y,inner_w,rh,dmw_color(0,0,128));int glyph_x=r.x+content_x-horizontal_offset;if(glyph_x>=r.x+2&&glyph_x<r.x+inner_w-12){const char *glyph=(nodes[i].flags&DMW_TREE_HAS_CHILDREN)?((nodes[i].flags&DMW_TREE_EXPANDED)?"-":"+"):" ";DMW_TEXT(glyph_x,y+rh-5,glyph,selected_row?dmw_color(255,255,255):dmw_color(0,0,0));}dmw_text_clip(draw_x,y+rh-5,nodes[i].text,crop,available,selected_row?dmw_color(255,255,255):dmw_color(0,0,0));}if(vbar)DMW_SCROLL(r.x+r.w-12,r.y+2,r.h-(hbar?14:4),10,1,count,rows,first_row);if(hbar)DMW_SCROLL(r.x+2,r.y+r.h-12,r.w-(vbar?14:4),10,0,max_width,inner_w-8,horizontal_offset);}
static inline int dmw_treeview_click(DmWidgetRect r,int x,int y,int count,int first_row,int rows,int has_vbar,int has_hbar){if(!dmw_contains(r,x,y)||x<r.x+2||x>=r.x+r.w-(has_vbar?12:2)||y<r.y+2||y>=r.y+r.h-(has_hbar?12:2))return -1;int row=(y-r.y-2)/dmw_treeview_row_height(),index=first_row+row;return row>=rows||index>=count?-1:index;}
static inline int dmw_treeview_toggle_click(DmWidgetRect r,int x,int y,const DmWidgetTreeNode *nodes,int count,int first_row,int rows,int has_vbar,int has_hbar,int horizontal_offset){int i=dmw_treeview_click(r,x,y,count,first_row,rows,has_vbar,has_hbar);if(i<0||!(nodes[i].flags&DMW_TREE_HAS_CHILDREN))return -1;int left=r.x+5+(int)nodes[i].depth*16-horizontal_offset;return x>=left&&x<left+14?i:-1;}
static inline int dmw_treeview_scroll_click(DmWidgetRect r,int x,int y,const DmWidgetTreeNode *nodes,int count,int *first_row,int *horizontal_offset){int rows=0,vbar=0,hbar=0,width=0;dmw_treeview_metrics(r,nodes,count,&rows,&vbar,&hbar,&width);if(vbar&&first_row&&dmw_scrollbar_click((DmWidgetRect){r.x+r.w-12,r.y+2,10,r.h-(hbar?14:4)},x,y,count,rows,first_row))return 1;int viewport=r.w-4-(vbar?12:0)-8;if(hbar&&horizontal_offset&&dmw_scrollbar_click((DmWidgetRect){r.x+2,r.y+r.h-12,r.w-(vbar?14:4),10},x,y,width,viewport,horizontal_offset))return 1;return 0;}
static inline void dmw_tree_item(int x,int y,int indent,const char *label,int selected,int expanded,int has_children){if(has_children)DMW_TEXT(x+indent,y+14,expanded?"-":"+",dmw_color(0,0,0));if(selected)DMW_RECT(x+indent+18,y,180,20,dmw_color(0,0,128));DMW_TEXT(x+indent+22,y+15,label,selected?dmw_color(255,255,255):dmw_color(0,0,0));}
static inline int dmw_tree_item_click(DmWidgetRect r,int x,int y){return dmw_contains(r,x,y);}
static inline void dmw_spin(DmWidgetRect r,int value,unsigned flags){DMW_RECT(r.x,r.y,r.w,r.h,dmw_color(255,255,255));dmw_border(r,1);DmWidgetRect up={r.x+r.w-18,r.y,18,r.h/2},down={r.x+r.w-18,r.y+r.h/2,18,r.h-r.h/2};dmw_button(up,"^",flags,0);dmw_button(down,"v",flags,0);char digits[12];int n=0,v=value;if(v<0){digits[n++]='-';v=-v;}char rev[10];int k=0;do{rev[k++]=(char)('0'+v%10);v/=10;}while(v&&k<9);while(k)digits[n++]=rev[--k];digits[n]=0;DMW_TEXT(r.x+4,r.y+r.h/2+5,digits,dmw_color(0,0,0));}
static inline int dmw_spin_click(DmWidgetRect r,int x,int y,unsigned flags,int *value,int minimum,int maximum){if((flags&DMW_DISABLED)||!value||!dmw_contains(r,x,y))return 0;if(x>=r.x+r.w-18){if(y<r.y+r.h/2&&*value<maximum)(*value)++;else if(y>=r.y+r.h/2&&*value>minimum)(*value)--;return 1;}return 0;}
static inline void dmw_tooltip(DmWidgetRect r,const char *text){DMW_RECT(r.x,r.y,r.w,r.h,dmw_color(255,255,225));dmw_border(r,0);DMW_TEXT(r.x+4,r.y+r.h/2+5,text,dmw_color(0,0,0));}
static inline void dmw_hotkey(DmWidgetRect r,const char *keys,unsigned flags,int focused){dmw_edit(r,keys,flags|(focused?DMW_FOCUSED:0),keys?(int)(DMW_WIDTH(keys)/8):0);}
static inline void dmw_animation_frame(DmWidgetRect r,unsigned frame,unsigned frame_count){DMW_RECT(r.x,r.y,r.w,r.h,dmw_color(192,192,192));if(frame_count){int width=(int)((uint64_t)r.w*(frame%frame_count+1)/frame_count);DMW_RECT(r.x+2,r.y+2,width>4?width-4:1,r.h-4,dmw_color(0,0,128));}}

#undef DMW_RECT
#undef DMW_TEXT
#undef DMW_WIDTH
#undef DMW_SCROLL
#endif
