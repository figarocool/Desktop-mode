#define DM_WIDGETS_WASM
#include "../app-sdk/dm_widgets.h"

void widget_api_compile_check(void){
    DmWidgetRect r={0,0,140,30};int value=10;const char*items[]={"A","B"};
    const char*tabs[]={"Uno","Due"};DmWidgetColumn columns[]={{"Nome",70}};const char*cells[]={"File"};
    dmw_label(r,"Etichetta",DM_WASM_COLOR_TEXT);dmw_button(r,"OK",DMW_DEFAULT,0);dmw_button_click(r,1,1,0);
    dmw_checkbox(r,"Attiva",1,0);dmw_checkbox_click(r,1,1,0,&value);dmw_radio(r,"Radio",1,0);dmw_radio_click(r,1,1,0,&value,1);
    dmw_groupbox(r,"Gruppo");dmw_edit(r,"Testo",DMW_MULTILINE,1);dmw_edit_click(r,1,1,0);
    dmw_listbox(r,items,2,1,0,DMW_MULTISELECT);dmw_listbox_ex(r,items,2,1,0,0,DMW_MULTISELECT);dmw_listbox_click(r,1,1,2,0,0);dmw_listbox_select(0,1,DMW_MULTISELECT);dmw_listbox_metrics(r,items,2,0,0,0,0);dmw_listbox_scroll_click(r,1,1,items,2,&value,&value);
    dmw_combo(r,"A",0,0);dmw_combo_click(r,1,1,0);dmw_combo_ex(r,items,2,1,0,0,2,0,1);dmw_combo_ex_click(r,1,1,items,2,0,2,1,0);dmw_progress(r,value,0,100,DMW_VERTICAL);
    dmw_trackbar(r,value,0,100,0);dmw_trackbar_click(r,1,1,0,100,0,&value);dmw_tabs(r,tabs,2,0);dmw_tabs_click(r,tabs,2,1,1);
    dmw_header(r,columns,1);dmw_listview_row(r,columns,cells,1,0,0);dmw_statusbar(r,items,2);dmw_toolbar_button(r,"Salva",0,0);
    DmWidgetTreeNode nodes[]={{"Nodo",0,DMW_TREE_HAS_CHILDREN|DMW_TREE_EXPANDED},{"Figlio",1,0}};dmw_tree_item(0,0,0,"Nodo",0,0,1);dmw_tree_item_click(r,1,1);dmw_treeview_metrics(r,nodes,2,0,0,0,0);dmw_treeview(r,nodes,2,0,0,0);dmw_treeview_click(r,1,1,2,0,2,0,0);dmw_treeview_toggle_click(r,1,1,nodes,2,0,2,0,0,0);dmw_treeview_scroll_click(r,1,1,nodes,2,&value,&value);dmw_spin(r,value,0);dmw_spin_click(r,1,1,0,&value,0,100);
    dmw_tooltip(r,"Aiuto");dmw_hotkey(r,"Ctrl+S",0,1);dmw_animation_frame(r,1,4);
    dmw_menubar(r,tabs,2,0);dmw_menubar_click(r,tabs,2,1,1);dmw_menu_popup(r,tabs,2,0,0);dmw_menu_popup_click(r,1,1,2,0);dmw_scrollbar_click(r,1,1,100,20,&value);
}
