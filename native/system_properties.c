#include "system_properties.h"
#include "desktop_ui.h"
#include <stdio.h>
#include <string.h>

#define DM_WIDGETS_NATIVE
#include "app-sdk/dm_widgets.h"

enum { TREE_CAPACITY=40, PROP_ICON_COUNT=15 };
enum {
    ICON_COMPUTER,ICON_PROCESSOR,ICON_MEMORY,ICON_DISPLAY,ICON_AUDIO,
    ICON_NETWORK,ICON_STORAGE,ICON_REMOVABLE,ICON_USB,ICON_GAMEPAD,
    ICON_CAMERA,ICON_KEYBOARD,ICON_MOUSE,ICON_BATTERY,ICON_BLUETOOTH
};
typedef struct {
    int id,parent,icon,depth,has_children,expanded;
    char label[96],detail[144],tree_label[104];
} PropertyNode;
typedef struct {
    DmSystemInfo info;
    unsigned elapsed;
    int tab,node_count,selected_id,first_row,horizontal_offset;
    int visible_count,visible_ids[TREE_CAPACITY];
    uint64_t last_click;
    int last_clicked_id;
    PropertyNode nodes[TREE_CAPACITY];
    DmWidgetTreeNode visible[TREE_CAPACITY];
    void *icons[PROP_ICON_COUNT];
} Properties;

static const char *tabs[]={"Generale","Gestione periferiche","Profili hardware","Prestazioni"};
static const char *icon_names[PROP_ICON_COUNT]={
    "prop-icon-computer","prop-icon-processor","prop-icon-memory","prop-icon-display",
    "prop-icon-audio","prop-icon-network","prop-icon-storage","prop-icon-removable",
    "prop-icon-usb","prop-icon-gamepad","prop-icon-camera","prop-icon-keyboard",
    "prop-icon-mouse","prop-icon-battery","prop-icon-bluetooth"
};
static const uint32_t ink=DM_COLOR(24,38,55,255);
static int is_tv_model(const Properties*p){return strstr(p->info.model,"TV")||strstr(p->info.model,"tv");}

static void refresh(Properties*p){dm_system_info(&p->info);}
static const char *asset_path(const char*name){
#ifdef DESKTOP_PREVIEW
    static char path[96];snprintf(path,sizeof(path),"native/assets/%s.png",name);return path;
#else
    static char path[96];snprintf(path,sizeof(path),"app0:/assets/%s.png",name);return path;
#endif
}
static void load_icons(Properties*p){for(int i=0;i<PROP_ICON_COUNT;i++)p->icons[i]=dm_image_load(asset_path(icon_names[i]));}
static void free_icons(Properties*p){for(int i=0;i<PROP_ICON_COUNT;i++)if(p->icons[i]){dm_image_free(p->icons[i]);p->icons[i]=NULL;}}
static int old_expanded(const PropertyNode*old,int count,int id,int fallback){for(int i=0;i<count;i++)if(old[i].id==id)return old[i].expanded;return fallback;}
static int add_node(Properties*p,int id,int parent,int icon,const char*label,const char*detail,int expanded){
    if(p->node_count>=TREE_CAPACITY)return -1;
    int index=p->node_count++;PropertyNode*n=&p->nodes[index];memset(n,0,sizeof(*n));
    n->id=id;n->parent=parent;n->icon=icon;n->depth=parent<0?0:p->nodes[parent].depth+1;
    n->expanded=expanded;
    snprintf(n->label,sizeof(n->label),"%s",label);
    snprintf(n->detail,sizeof(n->detail),"%s",detail?detail:label);
    snprintf(n->tree_label,sizeof(n->tree_label),"%s",label);
    if(parent>=0)p->nodes[parent].has_children=1;
    return index;
}
static int find_node(const Properties*p,int id){for(int i=0;i<p->node_count;i++)if(p->nodes[i].id==id)return i;return -1;}
static int add_child(Properties*p,int parent_id,int id,int icon,const char*label,const char*detail){int parent=find_node(p,parent_id);return parent<0?-1:add_node(p,id,parent,icon,label,detail,0);}
static const char *volume_name(const char*mount){
    if(!strcmp(mount,"ux0:"))return "Scheda di memoria ux0:";
    if(!strcmp(mount,"ur0:"))return "Memoria di sistema ur0:";
    if(!strcmp(mount,"imc0:"))return "Memoria interna imc0:";
    if(!strcmp(mount,"uma0:"))return "Unita USB uma0:";
    if(!strcmp(mount,"ud0:"))return "Unita USB ud0:";
    return mount;
}
static void build_device_tree(Properties*p){
    PropertyNode old[TREE_CAPACITY];int old_count=p->node_count;
    if(old_count)memcpy(old,p->nodes,(size_t)old_count*sizeof(*old));
    p->node_count=0;
    char value[144],title[96];
    snprintf(title,sizeof(title),"%.60s - DSKMODE01",p->info.model[0]?p->info.model:"PS Vita");
    add_node(p,1,-1,ICON_COMPUTER,title,"Sistema Desktop Mode su console PlayStation Vita",old_expanded(old,old_count,1,1));

    int root=1;
    snprintf(value,sizeof(value),"Batteria: %s",p->info.battery_percent>=0?"stato rilevato":"informazioni non disponibili");
    if(is_tv_model(p))add_node(p,10,0,ICON_BATTERY,dm_localize("Alimentazione","Power","Alimentacion"),"Alimentazione esterna PlayStation TV",old_expanded(old,old_count,10,0));
    else {
        snprintf(value,sizeof(value),"Batteria: %s",p->info.battery_percent>=0?"stato rilevato":"informazioni non disponibili");
        add_node(p,10,0,ICON_BATTERY,dm_localize("Batteria e alimentazione","Battery and power","Bateria y alimentacion"),value,old_expanded(old,old_count,10,0));
        if(p->info.battery_percent>=0){snprintf(value,sizeof(value),"%d%% | %s",p->info.battery_percent,p->info.charging?"In carica":"Alimentazione a batteria");add_child(p,10,11,ICON_BATTERY,"Batteria PS Vita",value);}
    }

    add_node(p,20,0,ICON_BLUETOOTH,dm_localize("Radio Bluetooth","Bluetooth radios","Radios Bluetooth"),"Radio Bluetooth integrata nella console",old_expanded(old,old_count,20,0));
    add_child(p,20,21,ICON_BLUETOOTH,"Adattatore Bluetooth","Integrato; configurazione dal pannello Bluetooth");

    if(!is_tv_model(p)){
        add_node(p,30,0,ICON_CAMERA,dm_localize("Fotocamere e sensori","Cameras and sensors","Camaras y sensores"),"Fotocamere e sensori integrati nel modello PS Vita",old_expanded(old,old_count,30,0));
        add_child(p,30,31,ICON_CAMERA,"Fotocamera anteriore","Componente integrato PS Vita");
        add_child(p,30,32,ICON_CAMERA,"Fotocamera posteriore","Componente integrato PS Vita");
        add_child(p,30,33,ICON_GAMEPAD,"Sensori di movimento","Accelerometro e giroscopio integrati");
    }

    add_node(p,40,0,ICON_GAMEPAD,dm_localize("Controller e interfacce HID","Controllers and HID interfaces","Mandos e interfaces HID"),"Controlli fisici e touch della console",old_expanded(old,old_count,40,0));
    if(!is_tv_model(p)){
        add_child(p,40,41,ICON_GAMEPAD,"Touchscreen frontale","Pannello capacitivo integrato");
        add_child(p,40,42,ICON_GAMEPAD,"Touchpad posteriore","Pannello touch capacitivo integrato");
    }
    add_child(p,40,43,ICON_GAMEPAD,"Stick analogici sinistro e destro","Comandi analogici integrati");
    add_child(p,40,44,ICON_GAMEPAD,"Croce direzionale e pulsanti","Tasti fisici della console");
    add_child(p,40,45,ICON_KEYBOARD,"Tastiera USB o Bluetooth","Periferica esterna, se collegata e supportata");
    add_child(p,40,46,ICON_MOUSE,"Mouse USB o Bluetooth","Periferica esterna, se collegata e supportata");

    add_node(p,50,0,ICON_NETWORK,dm_localize("Schede di rete","Network adapters","Adaptadores de red"),"Interfacce di rete della console",old_expanded(old,old_count,50,1));
    if(p->info.ip[0]&&strcmp(p->info.ip,"Wi-Fi non connesso"))snprintf(value,sizeof(value),"Wi-Fi | Connesso: %s",p->info.ip);
    else snprintf(value,sizeof(value),"Wi-Fi | Non connesso");
    add_child(p,50,51,ICON_NETWORK,"Adattatore Wi-Fi 802.11 b/g/n",value);
    add_child(p,50,52,ICON_BLUETOOTH,"Rete Bluetooth PAN","Interfaccia disponibile tramite stack Bluetooth");

    add_node(p,60,0,ICON_PROCESSOR,dm_localize("Processori","Processors","Procesadores"),"Processore e grafica della PS Vita",old_expanded(old,old_count,60,0));
    snprintf(value,sizeof(value),"CPU | %d MHz rilevati",p->info.cpu_mhz);add_child(p,60,61,ICON_PROCESSOR,"Processore ARM",value);
    snprintf(value,sizeof(value),"GPU | %d MHz rilevati",p->info.gpu_mhz);add_child(p,60,62,ICON_DISPLAY,"Processore grafico",value);

    add_node(p,70,0,ICON_DISPLAY,dm_localize("Schermo","Display","Pantalla"),"Schermo e uscita video Vita",old_expanded(old,old_count,70,0));
    add_child(p,70,71,ICON_DISPLAY,is_tv_model(p)?"Uscita video PlayStation TV":"Display PlayStation Vita",is_tv_model(p)?"Uscita video per TV":"Risoluzione ambiente: 960 x 544");

    add_node(p,80,0,ICON_STORAGE,dm_localize("Unita disco","Disk drives","Unidades de disco"),"Memorie montate nel file system Vita",old_expanded(old,old_count,80,1));
    static const char*mounts[]={"ux0:","ur0:","imc0:","uma0:","ud0:","vs0:","os0:","sa0:","gro0:","grw0:"};
    int mount_id=81;
    for(size_t i=0;i<sizeof(mounts)/sizeof(mounts[0]);i++){
        char path[24];snprintf(path,sizeof(path),"%s/",mounts[i]);
        if(!dm_fs_is_directory(path))continue;
        snprintf(value,sizeof(value),"Unita montata: %s",mounts[i]);
        add_child(p,80,mount_id++,ICON_REMOVABLE,volume_name(mounts[i]),value);
    }

    add_node(p,90,0,ICON_USB,dm_localize("Controller USB","USB controllers","Controladores USB"),"Porta USB e periferiche compatibili con il kernel Vita",old_expanded(old,old_count,90,1));
    add_child(p,90,91,ICON_USB,"Porta USB console","Periferiche USB secondo driver e supporto kernel");
    add_child(p,90,92,ICON_KEYBOARD,"Periferiche USB compatibili","Visibili se collegate e riconosciute dal sistema input");

    add_node(p,100,0,ICON_AUDIO,dm_localize("Audio","Sound, video and game controllers","Audio"),"Uscita audio e riproduzione multimediale",old_expanded(old,old_count,100,0));
    add_child(p,100,101,ICON_AUDIO,"Audio PlayStation Vita","Altoparlanti e uscita cuffie integrati");
    if(!is_tv_model(p))add_child(p,100,102,ICON_AUDIO,"Microfono","Ingresso audio integrato");

    add_node(p,110,0,ICON_MEMORY,dm_localize("Memoria","Memory","Memoria"),"Memoria disponibile per sistema e applicazioni",old_expanded(old,old_count,110,0));
    snprintf(value,sizeof(value),"RAM libera: %llu MiB",(unsigned long long)(p->info.ram_free/1048576));
    add_child(p,110,111,ICON_MEMORY,"Memoria di sistema",value);
    add_child(p,110,112,ICON_MEMORY,"Memoria grafica","Memoria video dedicata al processore grafico");

    add_node(p,120,0,ICON_COMPUTER,dm_localize("Dispositivi di sistema","System devices","Dispositivos del sistema"),"Hardware e servizi di base del sistema",old_expanded(old,old_count,120,0));
    snprintf(value,sizeof(value),"Firmware: %.80s",p->info.firmware[0]?p->info.firmware:"non disponibile");
    add_child(p,120,121,ICON_COMPUTER,"Sistema PlayStation Vita",value);
    p->selected_id=find_node(p,p->selected_id)>=0?p->selected_id:root;
}

static void build_visible(Properties*p){
    p->visible_count=0;
    int space_width=dm_text_width_raw(" ");if(space_width<1)space_width=1;
    int reserve_spaces=(18+space_width-1)/space_width;if(reserve_spaces<2)reserve_spaces=2;if(reserve_spaces>16)reserve_spaces=16;
    char indent[17];memset(indent,' ',(size_t)reserve_spaces);indent[reserve_spaces]=0;
    for(int i=0;i<p->node_count;i++){
        PropertyNode*n=&p->nodes[i];int show=1,parent=n->parent;
        while(parent>=0){if(!p->nodes[parent].expanded){show=0;break;}parent=p->nodes[parent].parent;}
        if(!show)continue;
        int row=p->visible_count++;
        p->visible_ids[row]=i;
        memcpy(n->tree_label,indent,(size_t)reserve_spaces);
        size_t label_length=strlen(n->label),available=sizeof(n->tree_label)-(size_t)reserve_spaces-1;
        if(label_length>available)label_length=available;
        memcpy(n->tree_label+reserve_spaces,n->label,label_length);n->tree_label[reserve_spaces+label_length]=0;
        p->visible[row]=(DmWidgetTreeNode){n->tree_label,(unsigned)n->depth,(n->has_children?DMW_TREE_HAS_CHILDREN:0)|(n->expanded?DMW_TREE_EXPANDED:0)};
    }
    if(p->first_row<0)p->first_row=0;
}

static void open_properties(DmWindow*w,const char*arg){
    (void)arg;Properties*p=w->state;refresh(p);load_icons(p);build_device_tree(p);build_visible(p);
}
static int close_properties(DmWindow*w){free_icons(w->state);return 1;}
static void text_at(DmWindow*w,int x,int y,const char*s){dm_text(w->x+x,w->y+y,s,ink);}
static void draw_fit(DmWindow*w,int x,int baseline,const char*s,int width,uint32_t color){
    char shown[180];snprintf(shown,sizeof(shown),"%.179s",s?s:"");
    while(shown[0]&&dm_text_width_raw(shown)>width){size_t n=strlen(shown);do{n--;}while(n&&((unsigned char)shown[n]&0xc0)==0x80);shown[n]=0;}
    dm_text_raw(w->x+x,w->y+baseline,shown,color);
}
static void draw_general(DmWindow*w,Properties*p){
    dm_rect(w->x+34,w->y+107,112,94,DM_COLOR(80,83,82,255));dm_rect(w->x+39,w->y+112,102,82,DM_COLOR(12,105,105,255));
    dm_rect(w->x+66,w->y+204,48,5,DM_COLOR(160,160,156,255));dm_rect(w->x+54,w->y+210,72,4,DM_COLOR(190,190,186,255));
    dm_rect(w->x+67,w->y+127,26,25,DM_COLOR(239,73,45,255));dm_rect(w->x+95,w->y+127,26,25,DM_COLOR(73,164,67,255));
    dm_rect(w->x+67,w->y+154,26,25,DM_COLOR(70,131,220,255));dm_rect(w->x+95,w->y+154,26,25,DM_COLOR(244,204,54,255));
    char value[128];text_at(w,202,100,"Sistema:");snprintf(value,sizeof(value),"Desktop Mode | API %d",DM_API_VERSION);text_at(w,222,124,value);
    snprintf(value,sizeof(value),"Firmware Vita: %.48s",p->info.firmware[0]?p->info.firmware:"non disponibile");text_at(w,222,147,value);
    text_at(w,202,180,"Sviluppato da:");text_at(w,222,204,"Stefano Basile");text_at(w,202,243,"Console:");
    text_at(w,222,267,p->info.model[0]?p->info.model:"PS Vita");text_at(w,222,290,"Ambiente desktop per PlayStation Vita");
}

static void draw_tree_icons(Properties*p,DmWidgetRect tree,int rows,int vbar,int hbar){
    int first=p->first_row;if(first<0)first=0;if(first>p->visible_count-rows)first=p->visible_count-rows;if(first<0)first=0;p->first_row=first;
    int row_height=dmw_treeview_row_height();int visible_rows=(tree.h-4-(hbar?12:0))/row_height;if(visible_rows<1)visible_rows=1;
    int max=first+visible_rows;if(max>p->visible_count)max=p->visible_count;
    for(int row=first;row<max;row++){
        PropertyNode*n=&p->nodes[p->visible_ids[row]];int x=tree.x+19+n->depth*16-p->horizontal_offset,y=tree.y+2+(row-first)*row_height+(row_height-16)/2;
        if(x>=tree.x+2&&x+16<=tree.x+tree.w-(vbar?12:2)&&p->icons[n->icon])dm_image_draw(p->icons[n->icon],x,y,16,16);
    }
}

static void draw_devices(DmWindow*w,Properties*p){
    build_visible(p);
    DmWidgetRect tree={w->x+18,w->y+78,w->w-36,w->h-174};
    int rows=0,vbar=0,hbar=0,width=0;dmw_treeview_metrics(tree,p->visible,p->visible_count,&rows,&vbar,&hbar,&width);
    int selected=-1;for(int i=0;i<p->visible_count;i++)if(p->nodes[p->visible_ids[i]].id==p->selected_id){selected=i;break;}
    dmw_treeview(tree,p->visible,p->visible_count,selected,p->first_row,p->horizontal_offset);
    draw_tree_icons(p,tree,rows,vbar,hbar);
    int node=find_node(p,p->selected_id);const char*detail=node>=0?p->nodes[node].detail:"";
    draw_fit(w,20,337,detail,w->w-40,ink);
    DmWidgetRect buttons[4]={{18,w->h-80,118,26},{142,w->h-80,118,26},{266,w->h-80,118,26},{390,w->h-80,118,26}};
    dmw_button(buttons[0],dm_localize("Proprietà","Properties","Propiedades"),selected<0?DMW_DISABLED:0,0);
    dmw_button(buttons[1],dm_localize("Aggiorna","Refresh","Actualizar"),0,0);
    dmw_button(buttons[2],dm_localize("Rimuovi","Remove","Quitar"),DMW_DISABLED,0);
    dmw_button(buttons[3],dm_localize("Stampa...","Print...","Imprimir..."),DMW_DISABLED,0);
}

static void draw_other_pages(DmWindow*w,Properties*p){
    if(p->tab==0){draw_general(w,p);return;}
    if(p->tab==1){draw_devices(w,p);return;}
    char lines[8][144],value[100];
    if(p->tab==2){
        snprintf(lines[0],144,"Console: %.80s",p->info.model);
        snprintf(lines[1],144,"Firmware: %.80s",p->info.firmware);
        snprintf(lines[2],144,"Sistema: PlayStation Vita / PlayStation TV");
        snprintf(lines[3],144,"Schermo: 960 x 544 pixel (coordinate ambiente)");
        snprintf(lines[4],144,"Input: controller, touch, USB/Bluetooth supportati");
        snprintf(lines[5],144,"Rete Wi-Fi: %.80s",p->info.ip);
        snprintf(lines[6],144,"Runtime applicazioni: WASM sandbox | API %d",DM_API_VERSION);
    }else{
        if(p->info.battery_percent>=0)snprintf(value,sizeof(value),"%d%% | %s",p->info.battery_percent,p->info.charging?"In carica":"A batteria");else snprintf(value,sizeof(value),"Non disponibile nell'anteprima PC");
        snprintf(lines[0],144,"Batteria: %s",value);snprintf(lines[1],144,"CPU: %d MHz",p->info.cpu_mhz);snprintf(lines[2],144,"GPU: %d MHz",p->info.gpu_mhz);
        snprintf(lines[3],144,"RAM libera: %llu MiB",(unsigned long long)(p->info.ram_free/1048576));
        snprintf(lines[4],144,"ux0: %llu MiB liberi / %llu MiB",(unsigned long long)(p->info.storage_free/1048576),(unsigned long long)(p->info.storage_total/1048576));
        snprintf(lines[5],144,"App esterne caricate: %d",dm_loaded_plugin_count());
        snprintf(lines[6],144,"Desktop Mode API: %d",DM_API_VERSION);
    }
    for(int i=0;i<7;i++)draw_fit(w,25,93+i*31,lines[i],w->w-50,ink);
    if(p->info.preview)draw_fit(w,25,327,"I dati hardware completi sono disponibili sulla PS Vita.",w->w-50,ink);
}

static void draw_properties(DmWindow*w){
    Properties*p=w->state;int x=12;
    for(int i=0;i<4;i++){
        const char*label=dm_localize(tabs[i],i==0?"General":i==1?"Device Manager":i==2?"Hardware Profiles":"Performance",i==0?"General":i==1?"Administrador de dispositivos":i==2?"Perfiles de hardware":"Rendimiento");
        int width=dm_text_width(label)+20;if(x+width>w->w-12)width=w->w-12-x;
        dm_rect(w->x+x,w->y+39,width,29,i==p->tab?DM_COLOR(242,239,229,255):DM_COLOR(198,198,194,255));
        dm_rect(w->x+x,w->y+66,width,1,DM_COLOR(242,239,229,255));dm_text_center(w->x+x+width/2,w->y+59,label,ink);x+=width+2;
    }
    dm_rect(w->x+12,w->y+67,w->w-24,w->h-119,DM_COLOR(211,208,200,255));draw_other_pages(w,p);
    DmWidgetRect ok={w->w-184,w->h-43,78,27},cancel={w->w-94,w->h-43,82,27};
    dmw_button(ok,"OK",DMW_DEFAULT,0);dmw_button(cancel,dm_localize("Annulla","Cancel","Cancelar"),0,0);
}

static void properties_key(DmWindow*w,int key){
    Properties*p=w->state;if(p->tab!=1)return;build_visible(p);if(!p->visible_count)return;
    int selected=0;for(int i=0;i<p->visible_count;i++)if(p->nodes[p->visible_ids[i]].id==p->selected_id){selected=i;break;}
    if(key==DM_KEY_UP||key==DM_KEY_DOWN){int visible_rows=(w->h-178)/dmw_treeview_row_height();if(visible_rows<1)visible_rows=1;selected+=key==DM_KEY_UP?-1:1;if(selected<0)selected=0;if(selected>=p->visible_count)selected=p->visible_count-1;p->selected_id=p->nodes[p->visible_ids[selected]].id;if(selected<p->first_row)p->first_row=selected;if(selected>=p->first_row+visible_rows)p->first_row=selected-visible_rows+1;return;}
    int source=p->visible_ids[selected];PropertyNode*n=&p->nodes[source];
    if(key==DM_KEY_RIGHT||key==DM_KEY_ENTER){if(n->has_children)n->expanded=1;}
    else if(key==DM_KEY_LEFT){if(n->has_children&&n->expanded)n->expanded=0;else if(n->parent>=0)p->selected_id=p->nodes[n->parent].id;}
    else if(key==DM_KEY_SCROLL_UP){if(p->first_row>0)p->first_row--;}
    else if(key==DM_KEY_SCROLL_DOWN&&p->first_row+1<p->visible_count)p->first_row++;
}

static void click_properties(DmWindow*w,int x,int y){
    Properties*p=w->state;int ax=x+w->x,ay=y+w->y;
    if(y>=39&&y<=68){int left=12;for(int i=0;i<4;i++){const char*label=dm_localize(tabs[i],i==0?"General":i==1?"Device Manager":i==2?"Hardware Profiles":"Performance",i==0?"General":i==1?"Administrador de dispositivos":i==2?"Perfiles de hardware":"Rendimiento");int width=dm_text_width(label)+20;if(x>=left&&x<left+width){p->tab=i;return;}left+=width+2;}}
    if(y>=w->h-47){dm_close(w);return;}
    if(p->tab==1){
        DmWidgetRect tree={w->x+18,w->y+78,w->w-36,w->h-174};build_visible(p);
        int rows=0,vbar=0,hbar=0,width=0;dmw_treeview_metrics(tree,p->visible,p->visible_count,&rows,&vbar,&hbar,&width);
        if(dmw_treeview_scroll_click(tree,ax,ay,p->visible,p->visible_count,&p->first_row,&p->horizontal_offset))return;
        int row=dmw_treeview_click(tree,ax,ay,p->visible_count,p->first_row,rows,vbar,hbar);if(row<0)return;
        int toggle=dmw_treeview_toggle_click(tree,ax,ay,p->visible,p->visible_count,p->first_row,rows,vbar,hbar,p->horizontal_offset);
        PropertyNode*n=&p->nodes[p->visible_ids[row]];p->selected_id=n->id;
        uint64_t now=dm_clock_ms();int double_click=p->last_clicked_id==n->id&&now-p->last_click<480;p->last_clicked_id=n->id;p->last_click=now;
        if(toggle>=0||double_click){n->expanded=!n->expanded;p->last_clicked_id=-1;build_visible(p);}
        DmWidgetRect buttons[4]={{18,w->h-80,118,26},{142,w->h-80,118,26},{266,w->h-80,118,26},{390,w->h-80,118,26}};
        if(dmw_button_click(buttons[0],ax,ay,n->id==p->selected_id?0:DMW_DISABLED)){dm_status(n->detail);return;}
        if(dmw_button_click(buttons[1],ax,ay,0)){refresh(p);build_device_tree(p);build_visible(p);dm_status(dm_localize("Elenco periferiche aggiornato","Device list refreshed","Lista de dispositivos actualizada"));return;}
    }
}

static void tick_properties(DmWindow*w,unsigned elapsed){Properties*p=w->state;p->elapsed+=elapsed;if(p->elapsed>=2000){p->elapsed=0;refresh(p);if(p->tab==1)build_device_tree(p);}}
const DmApp dm_system_properties_app={DM_API_VERSION,"properties","Proprietà - Sistema",sizeof(Properties),open_properties,draw_properties,click_properties,0,properties_key,close_properties,0,tick_properties};
