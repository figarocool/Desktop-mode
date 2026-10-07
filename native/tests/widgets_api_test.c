#define DM_PLUGIN_LINUX
#include "../app-sdk/dm_widgets.h"
#include <assert.h>
#include <string.h>

static int rectangles,texts,scrollbars;static char last_text[256];
static void rect(int x,int y,int w,int h,uint32_t c){(void)x;(void)y;(void)w;(void)h;(void)c;rectangles++;}
static void text(int x,int y,const char*s,uint32_t c){(void)x;(void)y;(void)c;texts++;strncpy(last_text,s?s:"",sizeof(last_text)-1);last_text[sizeof(last_text)-1]=0;}
static int width(const char*s){return (int)strlen(s)*8;}
static void scroll(int x,int y,int l,int t,int v,int c,int p,int o){(void)x;(void)y;(void)l;(void)t;(void)v;(void)c;(void)p;(void)o;scrollbars++;}
int main(void){
    static DmHostAPI host={.version=DM_API_VERSION,.struct_size=sizeof(DmHostAPI),.rect=rect,.text_raw=text,.text_width_raw=width,.scrollbar_draw=scroll};dm_api=&host;
    DmWidgetRect r={10,10,140,28};int value=0;
    dmw_button(r,"Apri",DMW_DEFAULT,0);assert(dmw_button_click(r,20,20,0));dmw_icon_button(r,DMW_ICON_PLAY,0,0);assert(dmw_icon_button_click(r,20,20,0));
    assert(dmw_checkbox_click(r,20,20,0,&value)&&value==1);dmw_checkbox(r,"Abilitato",value,0);
    assert(dmw_radio_click(r,20,20,0,&value,3)&&value==3);dmw_radio(r,"Scelta",1,0);
    dmw_groupbox((DmWidgetRect){0,0,180,80},"Gruppo");dmw_edit(r,"Testo",DMW_FOCUSED,2);
    const char*items[]={"Elemento molto lungo fuori dai bordi","Due","Tre","Quattro"};DmWidgetRect list={0,0,100,60};int rows=0,vbar=0,hbar=0,list_width=0,first=0,hscroll=0;dmw_listbox_metrics(list,items,4,&rows,&vbar,&hbar,&list_width);assert(vbar&&hbar&&rows>0&&list_width>list.w);dmw_listbox(list,items,4,1,0,DMW_MULTISELECT);dmw_listbox_ex(list,items,4,1,0,0,DMW_MULTISELECT);assert(dmw_listbox_click_ex(list,20,5,4,0,rows,vbar,hbar,0)==0);assert(dmw_listbox_scroll_click(list,90,40,items,4,&first,&hscroll)&&first>0);first=0;assert(dmw_listbox_scroll_click(list,80,55,items,4,&first,&hscroll)&&hscroll>0);assert(dmw_listbox_select(0,1,DMW_MULTISELECT)==2);
    dmw_combo(r,"Uno",0,0);dmw_combo_ex(r,items,4,1,0,0,2,0,1);assert(dmw_combo_ex_click(r,20,20,items,4,0,2,1,0)==-2);assert(dmw_combo_ex_click(r,20,45,items,4,0,2,1,0)==0);dmw_progress(r,50,0,100,0);dmw_progress(r,50,0,100,DMW_VERTICAL);
    dmw_trackbar(r,50,0,100,0);assert(dmw_trackbar_click(r,80,20,0,100,0,&value)&&value>0);
    const char*tabs[]={"Uno","Due"};dmw_tabs(r,tabs,2,0);assert(dmw_tabs_click(r,tabs,2,15,15)==0);
    const char*menus[]={"File","Modifica","Aiuto"};dmw_menubar(r,menus,3,1);assert(dmw_menubar_click(r,menus,3,20,20)==0);dmw_menu_popup(r,menus,3,1,4);assert(dmw_menu_popup_click(r,20,20,3,4)==0);int offset=0;assert(dmw_scrollbar_click((DmWidgetRect){0,0,10,100},5,75,500,100,&offset)&&offset>0);
    DmWidgetColumn columns[]={{"Nome",60},{"Tipo",60}};dmw_header(r,columns,2);const char*cells[]={"File.txt","Testo"};dmw_listview_row(r,columns,cells,2,1,0);
    const char*parts[]={"Pronto","ux0:"};dmw_statusbar(r,parts,2);dmw_toolbar_button(r,"Salva",0,1);dmw_text_clip(0,0,"Primo",0,80,0);assert(strcmp(last_text,"Primo")==0);
    DmWidgetTreeNode nodes[]={{"Radice",0,DMW_TREE_HAS_CHILDREN|DMW_TREE_EXPANDED},{"Cartella molto lunga da scorrere",1,DMW_TREE_HAS_CHILDREN},{"File.txt",2,0},{"Altro",0,0}};DmWidgetRect tree={0,0,100,60};int tree_rows=0,tree_v=0,tree_h=0,tree_width=0,tree_first=0,tree_x=0;dmw_treeview_metrics(tree,nodes,4,&tree_rows,&tree_v,&tree_h,&tree_width);assert(tree_v&&tree_h&&tree_rows>0);dmw_treeview(tree,nodes,4,0,0,0);assert(dmw_treeview_click(tree,20,5,4,0,tree_rows,tree_v,tree_h)==0);assert(dmw_treeview_toggle_click(tree,6,5,nodes,4,0,tree_rows,tree_v,tree_h,0)==0);assert(dmw_treeview_scroll_click(tree,90,40,nodes,4,&tree_first,&tree_x)&&tree_first>0);tree_first=0;assert(dmw_treeview_scroll_click(tree,80,55,nodes,4,&tree_first,&tree_x)&&tree_x>0);
    dmw_tree_item(0,0,10,"Cartella",1,1,1);assert(dmw_tree_item_click(r,20,20));dmw_spin(r,2,0);assert(dmw_spin_click(r,140,15,0,&value,0,10));dmw_tooltip(r,"Suggerimento");dmw_hotkey(r,"Ctrl+S",0,1);dmw_animation_frame(r,1,4);
    assert(rectangles>0&&texts>0&&scrollbars>0);return 0;
}
