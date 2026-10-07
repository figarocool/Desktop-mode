#include "file_icons.h"
#include "desktop_api.h"
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define INK DM_COLOR(24,38,55,255)
#define LIGHT DM_COLOR(224,235,247,255)
#define WHITE DM_COLOR(255,255,255,255)
typedef struct {
    char name[256];
    int dir;
}
DialogEntry;
static struct {
    int active,mode,count,offset,selected,valid_click,overwrite;
    uint64_t last_click;
    char directory[DM_PATH_MAX],filename[256],title[100],extensions[256],error[160],result[DM_PATH_MAX],rename_source[DM_PATH_MAX];
    DialogEntry *entries;
    DmFileResult callback;
    void*context;
}
dialog;
static int matches(const char*name) {
    if(!dialog.extensions[0])return 1;
    const char*ext=strrchr(name,'.');
    if(!ext)return 0;
    char filters[256];
    snprintf(filters,sizeof(filters),"%s",dialog.extensions);
    char*save,*part=strtok_r(filters,",",&save);
    while(part) {
        if(!dm_ascii_casecmp(ext,part))return 1;
        part=strtok_r(NULL,",",&save);
    }
    return 0;
}
static int sort_entries(const void*a,const void*b) {
    const DialogEntry*x=a,*y=b;
    return x->dir!=y->dir?y->dir-x->dir:dm_ascii_casecmp(x->name,y->name);
}
static void scan(void) {
    dialog.count=dialog.offset=dialog.valid_click=dialog.overwrite=0;
    dialog.selected=-1;
    dialog.error[0]=0;
    if(!dialog.directory[0]) {
        const char*mounts[]= {
            "ux0:","uma0:","imc0:","ur0:","ud0:","vs0:","os0:","sa0:","gro0:","grw0:"
        };
        for(unsigned i=0;i<sizeof(mounts)/sizeof(*mounts);i++) {
            char p[32];
            snprintf(p,sizeof(p),"%s/",mounts[i]);
            if(dm_fs_is_directory(p)) {
                DialogEntry*e=&dialog.entries[dialog.count++];
                snprintf(e->name,256,"%s",mounts[i]);
                e->dir=1;
            }
        }
        return;
    }
    int fd=sceIoDopen(dialog.directory);
    if(fd<0) {
        snprintf(dialog.error,sizeof(dialog.error),"Cartella non accessibile");
        return;
    }
    SceIoDirent entry;
    memset(&entry,0,sizeof(entry));
    while(dialog.count<2048&&sceIoDread(fd,&entry)>0) {
        int dir=SCE_S_ISDIR(entry.d_stat.st_mode);
        if(strcmp(entry.d_name,".")&&strcmp(entry.d_name,"..")&&(dir||matches(entry.d_name))) {
            DialogEntry*e=&dialog.entries[dialog.count++];
            snprintf(e->name,256,"%s",entry.d_name);
            e->dir=dir;
        }
        memset(&entry,0,sizeof(entry));
    }
    sceIoDclose(fd);
    qsort(dialog.entries,dialog.count,sizeof(*dialog.entries),sort_entries);
}
void dm_file_dialog_refresh(void){if(dialog.active)scan();}
int dm_file_dialog(const DmFileDialogOptions*o,DmFileResult callback,void*ctx) {
    if(!o||!callback||dialog.active||(o->mode!=DM_FILE_OPEN&&o->mode!=DM_FILE_SAVE))return -1;
    memset(&dialog,0,sizeof(dialog));
    dialog.entries=calloc(2048,sizeof(*dialog.entries));
    if(!dialog.entries)return -1;
    dialog.active=1;
    dialog.mode=o->mode;
    dialog.callback=callback;
    dialog.context=ctx;
    snprintf(dialog.title,sizeof(dialog.title),"%s",o->title?o->title:o->mode==DM_FILE_SAVE?"Salva con nome":"Apri file");
    snprintf(dialog.directory,sizeof(dialog.directory),"%s",o->initial_directory?o->initial_directory:"ux0:/data/");
    snprintf(dialog.filename,sizeof(dialog.filename),"%s",o->filename?o->filename:"");
    snprintf(dialog.extensions,sizeof(dialog.extensions),"%s",o->extensions?o->extensions:"");
    scan();
    return 0;
}
int dm_file_dialog_active(void) {
    return dialog.active;
}
void dm_file_dialog_cancel(void) {
    free(dialog.entries);
    memset(&dialog,0,sizeof(dialog));
}
static void finish(int replace) {
    DmFileResult callback=dialog.callback;
    void*ctx=dialog.context;
    char path[DM_PATH_MAX];
    snprintf(path,sizeof(path),"%s",dialog.result);
    dm_file_dialog_cancel();
    callback(path,replace,ctx);
}
static void accept(void) {
    if(!dialog.directory[0]||dm_fs_join(dialog.result,sizeof(dialog.result),dialog.directory,dialog.filename)) {
        strcpy(dialog.error,"Scegli una cartella e un nome file valido");
        return;
    }
    if(dm_fs_is_directory(dialog.result)) {
        snprintf(dialog.directory,sizeof(dialog.directory),"%s",dialog.result);
        scan();
        return;
    }
    int fd=sceIoOpen(dialog.result,SCE_O_RDONLY,0),exists=fd>=0;
    if(exists)sceIoClose(fd);
    if(dialog.mode==DM_FILE_OPEN) {
        if(!exists) {
            strcpy(dialog.error,"Il file non esiste");
            return;
        }
        finish(0);
        return;
    }
    if(!dm_fs_writable(dialog.result)) {
        strcpy(dialog.error,"Questa destinazione non e scrivibile");
        return;
    }
    if(exists) {
        dialog.overwrite=1;
        return;
    }
    finish(0);
}
static void filename_changed(const char*value,void*ctx) {
    (void)ctx;
    if(dialog.active) {
        snprintf(dialog.filename,sizeof(dialog.filename),"%s",value);
        dialog.error[0]=0;
    }
}
static void new_folder_created(const char*value,void*ctx) {
    (void)ctx;
    if(!dialog.active)return;
    char full[DM_PATH_MAX];
    if(!dm_fs_writable(dialog.directory)||dm_fs_join(full,sizeof(full),dialog.directory,value)||sceIoMkdir(full,0777)<0) {
        strcpy(dialog.error,"Impossibile creare la cartella");
        return;
    }
    snprintf(dialog.directory,sizeof(dialog.directory),"%s",full);
    scan();
}
static void rename_selected(const char*name,void*ctx){
    (void)ctx;if(!dialog.active)return;
    char target[DM_PATH_MAX];
    if(strlen(name)>=sizeof(dialog.filename)||dm_fs_join(target,sizeof(target),dialog.directory,name)||dm_fs_rename(dialog.rename_source,target)<0){
        strcpy(dialog.error,"Rinomina fallita: nome non valido o gia esistente");return;
    }
    const char*old=strrchr(dialog.rename_source,'/');
    if(old&&!strcmp(dialog.filename,old+1))snprintf(dialog.filename,sizeof(dialog.filename),"%s",name);
    scan();
    for(int i=0;i<dialog.count;i++)if(!strcmp(dialog.entries[i].name,name)){dialog.selected=i;dialog.offset=i/10*10;break;}
}
static int inside(int x,int y,int a,int b,int w,int h) {
    return x>=a&&x<a+w&&y>=b&&y<b+h;
}
void dm_file_dialog_click(int x,int y) {
    if(!dialog.active)return;
    if(dialog.overwrite) {
        if(inside(x,y,505,265,120,38))finish(1);
        else if(inside(x,y,340,265,140,38))dialog.overwrite=0;
        return;
    }
    if(inside(x,y,710,446,160,36)) {
        accept();
        return;
    }
    if(inside(x,y,90,446,120,36)) {
        dm_file_dialog_cancel();
        return;
    }
    if(inside(x,y,90,92,94,32)) {
        dm_fs_parent(dialog.directory);
        scan();
        return;
    }
    if(inside(x,y,540,92,115,32)) {
        if(dialog.selected<0||!dm_fs_writable(dialog.directory)){strcpy(dialog.error,"Seleziona un file o una cartella scrivibile");return;}
        DialogEntry*e=&dialog.entries[dialog.selected];
        if(!dm_fs_join(dialog.rename_source,sizeof(dialog.rename_source),dialog.directory,e->name))dm_prompt("Rinomina",e->name,rename_selected,NULL);
        return;
    }
    if(inside(x,y,665,92,100,32)) {
        snprintf(dialog.directory,sizeof(dialog.directory),"ux0:/data/desktop-mode/Desktop/");
        scan();
        return;
    }
    if(inside(x,y,780,92,90,32)) {
        dialog.directory[0]=0;
        scan();
        return;
    }
    if(inside(x,y,625,446,70,36)) {
        if(dm_fs_writable(dialog.directory))dm_prompt("Nuova cartella","Nuova cartella",new_folder_created,NULL);
        else strcpy(dialog.error,"Scegli prima una cartella scrivibile");
        return;
    }
    if(inside(x,y,240,402,630,30)) {
        dm_prompt("Nome file",dialog.filename,filename_changed,NULL);
        return;
    }
    if(inside(x,y,90,138,780,250)) {
        int idx=dialog.offset+(y-138)/25;
        if(idx>=dialog.count)return;
        int twice=dialog.valid_click&&idx==dialog.selected&&dm_clock_ms()-dialog.last_click<=450;
        dialog.selected=idx;
        dialog.valid_click=1;
        dialog.last_click=dm_clock_ms();
        DialogEntry*e=&dialog.entries[idx];
        if(e->dir) {
            if(twice) {
                char target[DM_PATH_MAX];
                if(!dialog.directory[0])snprintf(target,sizeof(target),"%s/",e->name);
                else if(dm_fs_join(target,sizeof(target),dialog.directory,e->name))return;
                snprintf(dialog.directory,sizeof(dialog.directory),"%s",target);
                scan();
            }
        }    else {
            snprintf(dialog.filename,sizeof(dialog.filename),"%s",e->name);
            if(twice&&dialog.mode==DM_FILE_OPEN)accept();
        }
    }
}
void dm_file_dialog_key(int key) {
    if(!dialog.active||dialog.overwrite)return;
    if(key==DM_KEY_ENTER)accept();
    if(key==DM_KEY_UP) {
        dialog.offset-=10;
        if(dialog.offset<0)dialog.offset=0;
    }
    if(key==DM_KEY_DOWN) {
        dialog.offset+=10;
        if(dialog.offset>=dialog.count)dialog.offset=dialog.count>10?dialog.count-10:0;
    }
}
void dm_file_dialog_draw(void) {
    if(!dialog.active)return;
    dm_rect(0,0,960,544,DM_COLOR(5,12,25,150));
    dm_rect(70,50,820,440,DM_COLOR(234,242,250,255));
    dm_rect(70,50,820,30,DM_COLOR(35,72,130,255));
    dm_text(85,72,dialog.title,WHITE);
    dm_rect(90,92,94,32,LIGHT);
    dm_text(100,115,"Indietro",INK);
    char p[80];
    snprintf(p,sizeof(p),"%.50s",dialog.directory[0]?dialog.directory:"Computer");
    while(strlen(p)&&dm_text_width_raw(p)>330)p[strlen(p)-1]=0;
    dm_text_raw(198,115,p,INK);
    dm_rect(540,92,115,32,LIGHT);
    dm_text(550,115,"Rinomina",INK);
    dm_rect(665,92,100,32,LIGHT);
    dm_text(680,115,"Desktop",INK);
    dm_rect(780,92,90,32,LIGHT);
    dm_text(790,115,"Unita",INK);
    for(int row=0;row<10&&dialog.offset+row<dialog.count;row++) {
        int idx=dialog.offset+row;
        DialogEntry*e=&dialog.entries[idx];
        dm_rect(90,138+row*25,780,24,dialog.selected==idx?DM_COLOR(160,203,247,255):WHITE);
        char icon_path[DM_PATH_MAX];
        int have_path=!dm_fs_join(icon_path,sizeof(icon_path),dialog.directory,e->name);
        if(e->dir||!have_path||!dm_file_icon(icon_path,105,140+row*25,20))dm_text(105,157+row*25,e->dir?"[+]":" -",INK);
        char label[90];
        snprintf(label,sizeof(label),"%.75s",e->name);
        dm_text_raw(140,157+row*25,label,INK);
    }
    dm_text(90,423,"Nome file:",INK);
    dm_rect(240,402,630,30,WHITE);
    dm_text_raw(250,423,dialog.filename,INK);
    dm_rect(90,446,120,36,LIGHT);
    dm_text(105,470,"Annulla",INK);
    dm_rect(625,446,70,36,LIGHT);
    dm_text(630,470,"+ Dir",INK);
    dm_rect(710,446,160,36,DM_COLOR(54,107,163,255));
    dm_text(740,470,dialog.mode==DM_FILE_SAVE?"Salva":"Apri",WHITE);
    dm_text(220,469,dialog.error[0]?dialog.error:"L/R: scorri | doppio clic: apri cartella",INK);
    if(dialog.overwrite) {
        dm_rect(0,0,960,544,DM_COLOR(0,0,0,140));
        dm_rect(270,180,420,140,WHITE);
        dm_text(290,214,"Il file esiste gia. Sostituirlo?",INK);
        dm_rect(340,265,140,38,LIGHT);
        dm_text(350,290,"Annulla",INK);
        dm_rect(505,265,120,38,DM_COLOR(171,65,65,255));
        dm_text(540,290,"Si",WHITE);
    }
}
