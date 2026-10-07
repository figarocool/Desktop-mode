#include "app_manager.h"
#include "file_jobs.h"
#include <stdio.h>
#include <string.h>
#define INK DM_COLOR(24,38,55,255)
typedef struct {
    int selected,offset;
    char pending_id[64],install_path[DM_PATH_MAX];
}
Manager;
static void removed(int yes,void*ctx) {
    DmWindow*w=ctx;
    if(yes&&w->used)dm_plugin_uninstall(((Manager*)w->state)->pending_id);
}
static void installed(int result,void*ctx) {
    DmWindow*w=ctx;
    if(!w->used)return;
    Manager*m=w->state;
    if(result==1)dm_plugin_install(m->install_path);
    else dm_status("Installazione non completata");
}
static void chosen(const char*path,int replace,void*ctx) {
    (void)replace;
    DmWindow*w=ctx;
    if(!w->used)return;
    Manager*m=w->state;
    const char*name=strrchr(path,'/');
    if(!name)return;
    if(dm_fs_join(m->install_path,sizeof(m->install_path),"ux0:/data/desktop-mode/apps/",name+1)<0)return;
    if(!strcmp(path,m->install_path)) {
        dm_plugin_install(path);
        return;
    }
    if(dm_job_copy(path,m->install_path,installed,w)<0)dm_status("Impossibile installare: nome gia presente o operazione attiva");
}
static void draw(DmWindow*w) {
    Manager*m=w->state;
    DmInstalledApp list[DM_APP_LIMIT];
    int count=dm_plugin_list(list,DM_APP_LIMIT);
    char headline[160];
    snprintf(headline,sizeof(headline),"Applicazioni Desktop Mode (.dmapp) | caricate: %d",dm_loaded_plugin_count());
    dm_text_raw(w->x+20,w->y+60,headline,INK);
    const char*labels[]={dm_localize("Installa...","Install...","Instalar..."),dm_localize("Disinstalla","Uninstall","Desinstalar"),dm_localize("Ripristina inclusa","Restore built-in","Restaurar incluida")};
    for(int i=0;i<3;i++) {
        dm_rect(w->x+20+i*210,w->y+74,200,30,DM_COLOR(208,222,237,255));
        dm_text(w->x+28+i*210,w->y+95,labels[i],INK);
    }
    int visible=(w->h-155)/30;
    for(int row=0;row<visible&&m->offset+row<count;row++) {
        int i=m->offset+row;
        DmInstalledApp*r=&list[i];
        char label[160];
        if(i==m->selected)dm_rect(w->x+20,w->y+120+row*30,w->w-40,28,DM_COLOR(151,195,246,255));
        snprintf(label,sizeof(label),"%s | %s%s",dm_ui_translate(r->app?r->app->title:"Modulo"),dm_localize(r->installed?"Installata":"Disinstallata",r->installed?"Installed":"Uninstalled",r->installed?"Instalada":"Desinstalada"),r->bundled?dm_localize(" | inclusa"," | built-in"," | incluida"):"");
        dm_text_raw(w->x+28,w->y+140+row*30,label,INK);
    }
    const char*diagnostic=dm_plugin_diagnostic();
    if(diagnostic&&diagnostic[0]){
        dm_text(w->x+20,w->y+w->h-42,"Ultimo errore di caricamento:",INK);
        dm_text_raw(w->x+20,w->y+w->h-22,diagnostic,INK);
    }else dm_text(w->x+20,w->y+w->h-20,"Chiudi le finestre prima di disinstallare. I documenti restano.",INK);
}
static void click(DmWindow*w,int x,int y) {
    Manager*m=w->state;
    DmInstalledApp list[DM_APP_LIMIT];
    int count=dm_plugin_list(list,DM_APP_LIMIT);
    if(y>=120&&y<w->h-35&&x>=20&&x<w->w-20) {
        int i=m->offset+(y-120)/30;
        if(i<count)m->selected=i;
        return;
    }
    if(y<74||y>=104||x<20||x>=650)return;
    int button=(x-20)/210;
    if(button==0) {
        DmFileDialogOptions o= {
            DM_FILE_OPEN,"Installa app .dmapp","ux0:/data/","",".dmapp"
        };
        dm_file_dialog(&o,chosen,w);
        return;
    }
    if(m->selected>=count)return;
    DmInstalledApp*r=&list[m->selected];
    if(!r->app)return;
    if(button==1&&r->installed) {
        snprintf(m->pending_id,sizeof(m->pending_id),"%s",r->app->id);
        dm_confirm("Disinstalla applicazione","Rimuovere l'app da Start e dalle associazioni? I documenti restano.",removed,w);
    }
    if(button==2&&!r->installed)dm_plugin_reinstall(r->app->id);
}
static void key(DmWindow*w,int k) {
    Manager*m=w->state;
    DmInstalledApp list[DM_APP_LIMIT];
    int n=dm_plugin_list(list,DM_APP_LIMIT);
    if(k==DM_KEY_UP&&m->offset)m->offset--;
    if(k==DM_KEY_DOWN&&m->offset+(w->h-155)/30<n)m->offset++;
}
const DmApp dm_app_manager_app= {
    DM_API_VERSION,"apps","Applicazioni installate",sizeof(Manager),0,draw,click,0,key,0,0,0
};
