#include "file_types.h"
#include "app_manager.h"
#include "preferences.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
static struct {char extension[24],app[64];} choices[64];
static int choice_count;
static char settings_path[DM_PATH_MAX]="ux0:/data/desktop-mode/associations.ini";
int dm_extension_valid(const char*e){if(!e||e[0]!='.'||!e[1]||strlen(e)>=24)return 0;for(const unsigned char*p=(const unsigned char*)e+1;*p;p++)if(!isalnum(*p)&&*p!='_'&&*p!='-')return 0;return 1;}
int dm_extension_reserved(const char*e){return e&&(!dm_ascii_casecmp(e,".dmapp")||!dm_ascii_casecmp(e,".dmsaver")||!dm_ascii_casecmp(e,".dmlink"));}
static const DmApp*find_app(const char*id){for(int i=0;i<dm_registered_count();i++){const DmApp*a=dm_registered_app(i);if(!strcmp(a->id,id))return a;}return NULL;}
const char*dm_association_app(const char*e){if(!dm_extension_valid(e)||dm_extension_reserved(e))return NULL;for(int i=0;i<choice_count;i++)if(!dm_ascii_casecmp(choices[i].extension,e)&&find_app(choices[i].app))return choices[i].app;return dm_association_default(e);}
static int save(void){
#ifdef DESKTOP_PREVIEW
 if(getenv("DESKTOP_SELF_TEST")&&!strcmp(settings_path,"ux0:/data/desktop-mode/associations.ini"))return 0;
#endif
 char data[64*90];size_t n=0;for(int i=0;i<choice_count;i++)n+=(size_t)snprintf(data+n,sizeof(data)-n,"%s=%s\n",choices[i].extension,choices[i].app);return dm_fs_write(settings_path,data,n,0);
}
int dm_association_choose(const char*e,const char*id){if(!dm_extension_valid(e)||dm_extension_reserved(e)||!id||strlen(id)>=64||!find_app(id))return -1;for(const char*p=id;*p;p++)if(!isalnum((unsigned char)*p)&&*p!='_'&&*p!='-')return -1;int i;for(i=0;i<choice_count;i++)if(!dm_ascii_casecmp(choices[i].extension,e))break;if(i==64)return -1;char old[64];snprintf(old,sizeof(old),"%s",i<choice_count?choices[i].app:"");int added=i==choice_count;snprintf(choices[i].extension,24,"%s",e);for(char*p=choices[i].extension;*p;p++)*p=(char)tolower((unsigned char)*p);snprintf(choices[i].app,64,"%s",id);if(added)choice_count++;if(save()<0){if(added)choice_count--;else snprintf(choices[i].app,64,"%s",old);dm_status("Impossibile salvare le associazioni");return -1;}return 0;}
int dm_association_reset(const char*e){if(!dm_extension_valid(e)||dm_extension_reserved(e))return -1;for(int i=0;i<choice_count;i++)if(!dm_ascii_casecmp(choices[i].extension,e)){char ext[24],id[64];strcpy(ext,choices[i].extension);strcpy(id,choices[i].app);memmove(choices+i,choices+i+1,(choice_count-i-1)*sizeof(*choices));choice_count--;if(save()<0){strcpy(choices[choice_count].extension,ext);strcpy(choices[choice_count++].app,id);return -1;}break;}return 0;}
void dm_associations_load(void){choice_count=0;
#ifdef DESKTOP_PREVIEW
 if(getenv("DESKTOP_SELF_TEST")&&!strcmp(settings_path,"ux0:/data/desktop-mode/associations.ini"))return;
#endif
 char data[64*90+1];if(dm_fs_read(settings_path,data,sizeof(data))<0)return;char*saveptr;for(char*line=strtok_r(data,"\r\n",&saveptr);line&&choice_count<64;line=strtok_r(NULL,"\r\n",&saveptr)){char*eq=strchr(line,'=');if(!eq)continue;*eq++=0;if(!dm_extension_valid(line)||dm_extension_reserved(line)||!eq[0]||strlen(eq)>=64)continue;int valid=1;for(char*p=eq;*p;p++)if(!isalnum((unsigned char)*p)&&*p!='_'&&*p!='-')valid=0;if(!valid)continue;int i;for(i=0;i<choice_count;i++)if(!dm_ascii_casecmp(choices[i].extension,line))break;snprintf(choices[i].extension,24,"%s",line);snprintf(choices[i].app,64,"%s",eq);if(i==choice_count)choice_count++;}}
#ifdef DESKTOP_PREVIEW
void dm_associations_test_path(const char*p){snprintf(settings_path,sizeof(settings_path),"%s",p?p:"ux0:/data/desktop-mode/associations.ini");dm_associations_load();}
#endif
/* Include user extensions as well as installed defaults; reserved rows remain visible and locked. */
static int extensions(char out[131][24]){int n=0;for(int i=0;i<dm_association_count();i++)snprintf(out[n++],24,"%s",dm_association_extension(i));for(int i=0;i<choice_count;i++){int j;for(j=0;j<n;j++)if(!dm_ascii_casecmp(out[j],choices[i].extension))break;if(j==n)snprintf(out[n++],24,"%.23s",choices[i].extension);}strcpy(out[n++],".dmapp");strcpy(out[n++],".dmsaver");strcpy(out[n++],".dmlink");return n;}
typedef struct {int selected,offset,app_offset;char extension[24];} Types;
static void button(DmWindow*w,int x,int y,int width,const char*label){dm_rect(w->x+x,w->y+y,width,30,DM_COLOR(195,216,238,255));dm_text(w->x+x+8,w->y+y+22,dm_ui_translate(label),DM_COLOR(24,38,55,255));}
static void draw(DmWindow*w){Types*s=w->state;char list[131][24];int n=extensions(list),rows=(w->h-170)/27;int selected=-1;for(int i=0;i<n;i++)if(!dm_ascii_casecmp(list[i],s->extension))selected=i;if(selected<0){selected=0;snprintf(s->extension,24,"%s",list[0]);}s->selected=selected;if(rows<1)rows=1;dm_text(w->x+20,w->y+60,"Estensione         Applicazione",DM_COLOR(24,38,55,255));for(int r=0;r<rows&&s->offset+r<n;r++){int i=s->offset+r;const char*id=dm_association_app(list[i]);const DmApp*a=id?find_app(id):NULL;dm_rect(w->x+16,w->y+72+r*27,w->w-32,26,i==s->selected?DM_COLOR(169,208,245,255):DM_COLOR(244,248,251,255));dm_text(w->x+25,w->y+92+r*27,list[i],DM_COLOR(24,38,55,255));dm_text(w->x+170,w->y+92+r*27,dm_ui_translate(dm_extension_reserved(list[i])?"Desktop Mode (protetta)":a?a->title:"Nessuna applicazione"),DM_COLOR(24,38,55,255));}int y=w->h-88;button(w,20,y,110,"Precedenti");button(w,140,y,110,"Successive");button(w,270,y,125,"Cambia app");button(w,405,y,110,"Ripristina");button(w,20,w->h-48,220,"Aggiungi estensione...");dm_text(w->x+260,w->y+w->h-26,".dmapp / .dmsaver / .dmlink: protette",DM_COLOR(24,38,55,255));}
static void chosen(const char*value,void*context){(void)context;if(!value)return;char extension[24];snprintf(extension,sizeof(extension),"%s",value);if(!dm_extension_valid(extension)||dm_extension_reserved(extension)){dm_status("Estensione non valida o protetta");return;}dm_launch("choosefileapp",extension);}
static void click(DmWindow*w,int x,int y){Types*s=w->state;char list[131][24];int n=extensions(list),rows=(w->h-170)/27;if(rows<1)rows=1;if(y>=72&&y<72+rows*27){int i=s->offset+(y-72)/27;if(i<n){s->selected=i;snprintf(s->extension,24,"%s",list[i]);}return;}if(y>=w->h-88&&y<w->h-58){if(x>=20&&x<130)s->offset=s->offset>rows?s->offset-rows:0;if(x>=140&&x<250&&s->offset+rows<n)s->offset+=rows;if(x>=270&&x<395){if(dm_extension_reserved(s->extension))dm_status("Questa estensione e protetta");else if(s->extension[0])dm_launch("choosefileapp",s->extension);}if(x>=405&&x<515&&s->extension[0])dm_association_reset(s->extension);}if(y>=w->h-48&&y<w->h-18&&x>=20&&x<240)dm_prompt("Estensione (esempio: .txt)",".",chosen,NULL);}
static void open(DmWindow*w,const char*arg){(void)arg;Types*s=w->state;const char*ext=dm_association_extension(0);if(ext)snprintf(s->extension,24,"%s",ext);}
const DmApp dm_file_types_app={DM_API_VERSION,"filetypes","Programmi predefiniti",sizeof(Types),open,draw,click,NULL,NULL,NULL,NULL,NULL};
static int choose_apps(const DmApp**out){DmInstalledApp modules[24];int n=dm_plugin_list(modules,24),count=0;for(int i=0;i<n;i++)if(modules[i].app&&find_app(modules[i].app->id))out[count++]=modules[i].app;return count;}
typedef struct {char ext[24];int offset;} Choose;
static void choose_open(DmWindow*w,const char*arg){Choose*s=w->state;if(arg)snprintf(s->ext,24,"%s",arg);}
static void choose_draw(DmWindow*w){Choose*s=w->state;const DmApp*apps[24];int count=choose_apps(apps);char label[80];snprintf(label,sizeof(label),"Apri %s con:",s->ext);dm_text(w->x+20,w->y+60,label,DM_COLOR(24,38,55,255));int rows=(w->h-140)/30;for(int r=0;r<rows&&s->offset+r<count;r++){const DmApp*a=apps[s->offset+r];button(w,20,75+r*30,w->w-40,a->title);}button(w,20,w->h-48,120,"Precedenti");button(w,150,w->h-48,120,"Successive");}
static void choose_click(DmWindow*w,int x,int y){Choose*s=w->state;const DmApp*apps[24];int count=choose_apps(apps);int rows=(w->h-140)/30;if(x>=20&&x<w->w-20&&y>=75&&y<75+rows*30){int index=s->offset+(y-75)/30;const DmApp*a=index<count?apps[index]:NULL;if(a&&!dm_association_choose(s->ext,a->id))dm_close(w);return;}if(y>=w->h-48&&y<w->h-18){if(x>=20&&x<140)s->offset=s->offset>rows?s->offset-rows:0;if(x>=150&&x<270&&s->offset+rows<count)s->offset+=rows;}}
const DmApp dm_choose_file_app={DM_API_VERSION,"choosefileapp","Scegli applicazione",sizeof(Choose),choose_open,choose_draw,choose_click,NULL,NULL,NULL,NULL,NULL};
