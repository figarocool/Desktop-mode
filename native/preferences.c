#include "preferences.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <psp2/io/stat.h>
#define PREF_FILE "ux0:/data/desktop-mode/preferences.ini"
DmPreferences dm_preferences={56,85,DM_LANG_SYSTEM,1,300,0,0,"native"};
static int system_language=DM_LANG_IT;
#ifndef DESKTOP_PREVIEW
static int original_cpu,original_gpu;
#endif
#ifdef DESKTOP_PREVIEW
#include <SDL.h>
int dm_clock_set(int cpu,int gpu){(void)cpu;(void)gpu;dm_status("Frequenze Vita non disponibili nell'anteprima");return -1;}
int dm_clock_restore(void){return -1;}
#else
#include <psp2/power.h>
#include <psp2/apputil.h>
#include <psp2/system_param.h>
static int util_ready(void){static int initialized;if(initialized)return 0;SceAppUtilInitParam init={0};SceAppUtilBootParam boot={0};int r=sceAppUtilInit(&init,&boot);if(r>=0)initialized=1;return r;}
int dm_clock_set(int cpu,int gpu){int cpus[]={111,166,222,333,444},gpus[]={111,166,222};int okcpu=0,okgpu=0;for(unsigned i=0;i<sizeof(cpus)/sizeof(*cpus);i++)okcpu|=cpu==cpus[i];for(unsigned i=0;i<sizeof(gpus)/sizeof(*gpus);i++)okgpu|=gpu==gpus[i];if(!okcpu||!okgpu)return -1;int oldcpu=scePowerGetArmClockFrequency(),oldgpu=scePowerGetGpuClockFrequency();if(scePowerSetArmClockFrequency(cpu)<0)return -1;if(scePowerSetGpuClockFrequency(gpu)<0){scePowerSetArmClockFrequency(oldcpu);return -1;}if(scePowerGetArmClockFrequency()!=cpu||scePowerGetGpuClockFrequency()!=gpu){scePowerSetArmClockFrequency(oldcpu);scePowerSetGpuClockFrequency(oldgpu);return -1;}return 0;}
int dm_clock_restore(void){int a=scePowerSetArmClockFrequency(original_cpu),b=scePowerSetGpuClockFrequency(original_gpu);return a<0||b<0?-1:0;}
#endif
int dm_language(void){return dm_preferences.language==DM_LANG_SYSTEM?system_language:dm_preferences.language;}
const char*dm_localize(const char*it,const char*en,const char*es){int l=dm_language();return l==DM_LANG_EN&&en?en:l==DM_LANG_ES&&es?es:it;}
int dm_preferences_save(void){
#ifdef DESKTOP_PREVIEW
 if(getenv("DESKTOP_SELF_TEST"))return 0;
#endif
 char text[300];int n=snprintf(text,sizeof(text),"icon=%d\nfont=%d\nlanguage=%d\nsaver_enabled=%d\nsaver_seconds=%d\ncpu=%d\ngpu=%d\nsaver=%s\n",dm_preferences.icon_size,dm_preferences.font_percent,dm_preferences.language,dm_preferences.saver_enabled,dm_preferences.saver_seconds,dm_preferences.cpu_mhz,dm_preferences.gpu_mhz,dm_preferences.saver);return dm_fs_write(PREF_FILE,text,n,0);
}
int dm_language_set(int language){if(language<DM_LANG_SYSTEM||language>DM_LANG_ES)return -1;dm_preferences.language=language;return dm_preferences_save();}
int dm_appearance_set(int icon,int font){if(icon<32||icon>80||font<70||font>110)return -1;dm_preferences.icon_size=icon;dm_preferences.font_percent=font;return dm_preferences_save();}
void dm_appearance_reset(void){dm_appearance_set(56,85);}
void dm_preferences_init(void){
#ifndef DESKTOP_PREVIEW
 original_cpu=scePowerGetArmClockFrequency();original_gpu=scePowerGetGpuClockFrequency();int language;if(util_ready()>=0&&sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_LANG,&language)>=0)system_language=language==SCE_SYSTEM_PARAM_LANG_ITALIAN?DM_LANG_IT:language==SCE_SYSTEM_PARAM_LANG_SPANISH?DM_LANG_ES:DM_LANG_EN;
#else
 const char*lang=getenv("LANG");if(lang)system_language=!strncmp(lang,"it",2)?DM_LANG_IT:!strncmp(lang,"es",2)?DM_LANG_ES:DM_LANG_EN;
 if(getenv("DESKTOP_SELF_TEST")){system_language=DM_LANG_IT;return;}
#endif
 char text[1024];if(dm_fs_read(PREF_FILE,text,sizeof(text))<0)return;char*save,*line=strtok_r(text,"\n",&save);while(line){int n;if(sscanf(line,"icon=%d",&n)==1&&n>=32&&n<=80)dm_preferences.icon_size=n;else if(sscanf(line,"font=%d",&n)==1&&n>=70&&n<=110)dm_preferences.font_percent=n;else if(sscanf(line,"language=%d",&n)==1&&n>=-1&&n<=2)dm_preferences.language=n;else if(sscanf(line,"saver_enabled=%d",&n)==1&&(n==0||n==1))dm_preferences.saver_enabled=n;else if(sscanf(line,"saver_seconds=%d",&n)==1&&n>=10&&n<=3600)dm_preferences.saver_seconds=n;else if(sscanf(line,"cpu=%d",&n)==1&&(n==111||n==166||n==222||n==333||n==444))dm_preferences.cpu_mhz=n;else if(sscanf(line,"gpu=%d",&n)==1&&(n==111||n==166||n==222))dm_preferences.gpu_mhz=n;else if(!strncmp(line,"saver=",6)&&strlen(line+6)<64)snprintf(dm_preferences.saver,64,"%s",line+6);line=strtok_r(NULL,"\n",&save);}
 if(dm_preferences.cpu_mhz&&dm_preferences.gpu_mhz&&dm_clock_set(dm_preferences.cpu_mhz,dm_preferences.gpu_mhz)<0)dm_status("Profilo frequenze non applicato dal sistema");
}
typedef struct {int page,cpu,gpu;} Settings;
static void open_settings(DmWindow*w,const char*a){(void)a;Settings*s=w->state;DmSystemInfo i;dm_system_info(&i);s->cpu=i.cpu_mhz>0?i.cpu_mhz:333;s->gpu=i.gpu_mhz>0?i.gpu_mhz:166;}
static void button(DmWindow*w,int x,int y,int width,const char*text){dm_rect(w->x+x,w->y+y,width,32,DM_COLOR(202,222,243,255));dm_text(w->x+x+8,w->y+y+22,dm_ui_translate(text),DM_COLOR(24,38,55,255));}
static void draw_settings(DmWindow*w){Settings*s=w->state;unsigned ink=DM_COLOR(24,38,55,255);const char*tabs[]={"Prestazioni","Aspetto","Lingua","Screensaver"};for(int i=0;i<4;i++)button(w,15+i*165,38,158,tabs[i]);char text[180];
 if(s->page==0){DmSystemInfo i;dm_system_info(&i);snprintf(text,sizeof(text),"CPU: %d MHz   GPU: %d MHz",s->cpu,s->gpu);dm_text(w->x+25,w->y+110,text,ink);button(w,25,130,90,"CPU -");button(w,125,130,90,"CPU +");button(w,240,130,90,"GPU -");button(w,340,130,90,"GPU +");button(w,25,190,160,"Applica");button(w,200,190,230,"Ripristina frequenze");snprintf(text,sizeof(text),"%s: CPU %d MHz | GPU %d MHz",dm_localize("Valori letti","Readback","Valores leidos"),i.cpu_mhz,i.gpu_mhz);dm_text(w->x+25,w->y+270,text,ink);dm_text(w->x+25,w->y+315,dm_localize("500 MHz richiede un plugin overclock kernel dedicato.","500 MHz requires a dedicated kernel overclock plugin.","500 MHz requiere un plugin kernel de overclock."),ink);}
 if(s->page==1){snprintf(text,sizeof(text),"%s: %d px",dm_ui_translate("Dimensione icone"),dm_preferences.icon_size);dm_text(w->x+25,w->y+110,text,ink);button(w,25,130,80,"-");button(w,115,130,80,"+");button(w,220,130,190,"Reset icone");snprintf(text,sizeof(text),"%s: %d%%",dm_ui_translate("Dimensione caratteri"),dm_preferences.font_percent);dm_text(w->x+25,w->y+210,text,ink);button(w,25,230,80,"-");button(w,115,230,80,"+");button(w,220,230,190,"Reset caratteri");button(w,25,295,240,"Reset aspetto originale");}
 if(s->page==2){const char*choices[]={"Lingua della console","Italiano","English","Español"};for(int i=0;i<4;i++){button(w,25,100+i*50,320,choices[i]);if(dm_preferences.language==i-1)dm_text(w->x+360,w->y+122+i*50,"*",ink);}dm_text(w->x+25,w->y+340,dm_localize("Le app possono usare la lingua scelta tramite API.","Apps can use the selected language through the API.","Las apps pueden usar el idioma seleccionado mediante API."),ink);}
 if(s->page==3){button(w,25,95,210,dm_preferences.saver_enabled?"Disabilita screensaver":"Abilita screensaver");snprintf(text,sizeof(text),"%s: %d s",dm_ui_translate("Tempo inattivita"),dm_preferences.saver_seconds);dm_text(w->x+25,w->y+175,text,ink);button(w,25,190,80,"-");button(w,115,190,80,"+");button(w,220,190,160,"Imposta secondi");const DmScreenSaver*chosen=NULL;for(int i=0;i<dm_saver_count();i++)if(!strcmp(dm_saver_at(i)->id,dm_preferences.saver))chosen=dm_saver_at(i);button(w,25,245,340,chosen?chosen->title:"Desktop Mode");button(w,380,245,150,"Aggiorna");button(w,25,300,170,"Anteprima");dm_text(w->x+25,w->y+365,"ux0:/data/desktop-mode/screensavers/",ink);}
}
static void seconds_chosen(const char*text,void*ctx){DmWindow*w=ctx;if(!w->used)return;char*end;long n=strtol(text,&end,10);if(!*text||*end||n<10||n>3600){dm_status(dm_localize("Intervallo: 10-3600 secondi","Range: 10-3600 seconds","Intervalo: 10-3600 segundos"));return;}dm_preferences.saver_seconds=n;dm_preferences_save();}
static int cycle(int current,const int*list,int count,int direction){int index=0;for(int i=0;i<count;i++)if(list[i]==current)index=i;index+=direction;if(index<0)index=0;if(index>=count)index=count-1;return list[index];}
static void click_settings(DmWindow*w,int x,int y){Settings*s=w->state;if(y>=38&&y<70&&x>=15&&x<675){s->page=(x-15)/165;return;}if(s->page==0){int cpu[]={111,166,222,333,444},gpu[]={111,166,222};if(y>=130&&y<162){if(x>=25&&x<215)s->cpu=cycle(s->cpu,cpu,5,x<115?-1:1);if(x>=240&&x<430)s->gpu=cycle(s->gpu,gpu,3,x<330?-1:1);}if(y>=190&&y<222){if(x>=25&&x<185){if(dm_clock_set(s->cpu,s->gpu)<0)dm_status(dm_localize("Frequenze non applicate: API non disponibile o plugin le impone","Clocks not applied: unavailable API or plugin override","Frecuencias no aplicadas: API no disponible o plugin activo"));else{dm_preferences.cpu_mhz=s->cpu;dm_preferences.gpu_mhz=s->gpu;dm_preferences_save();}}if(x>=200&&x<430){if(dm_clock_restore()>=0){dm_preferences.cpu_mhz=dm_preferences.gpu_mhz=0;dm_preferences_save();}open_settings(w,NULL);}}}
 if(s->page==1){if(y>=130&&y<162){if(x>=25&&x<105)dm_appearance_set(dm_preferences.icon_size>32?dm_preferences.icon_size-8:32,dm_preferences.font_percent);if(x>=115&&x<195)dm_appearance_set(dm_preferences.icon_size<80?dm_preferences.icon_size+8:80,dm_preferences.font_percent);if(x>=220&&x<410)dm_appearance_set(56,dm_preferences.font_percent);}if(y>=230&&y<262){if(x>=25&&x<105)dm_appearance_set(dm_preferences.icon_size,dm_preferences.font_percent>70?dm_preferences.font_percent-5:70);if(x>=115&&x<195)dm_appearance_set(dm_preferences.icon_size,dm_preferences.font_percent<110?dm_preferences.font_percent+5:110);if(x>=220&&x<410)dm_appearance_set(dm_preferences.icon_size,85);}if(y>=295&&y<327&&x>=25&&x<265)dm_appearance_reset();}
 if(s->page==2&&x>=25&&x<345&&y>=100&&y<300){int i=(y-100)/50;if(y%50<32)dm_language_set(i-1);}
 if(s->page==3){if(y>=95&&y<127&&x>=25&&x<235){dm_preferences.saver_enabled=!dm_preferences.saver_enabled;dm_saver_wake();}if(y>=190&&y<222){if(x>=25&&x<105)dm_preferences.saver_seconds=dm_preferences.saver_seconds>30?dm_preferences.saver_seconds-30:10;if(x>=115&&x<195)dm_preferences.saver_seconds=dm_preferences.saver_seconds<3570?dm_preferences.saver_seconds+30:3600;if(x>=220&&x<380){char text[20];snprintf(text,sizeof(text),"%d",dm_preferences.saver_seconds);dm_prompt("Tempo inattivita (10-3600 s)",text,seconds_chosen,w);}}if(y>=245&&y<277){if(x>=25&&x<365){int n=dm_saver_count(),index=0;for(int i=0;i<n;i++)if(!strcmp(dm_saver_at(i)->id,dm_preferences.saver))index=i;const DmScreenSaver*next=dm_saver_at((index+1)%n);snprintf(dm_preferences.saver,64,"%s",next->id);}if(x>=380&&x<530)dm_scan_savers();}if(y>=300&&y<332&&x>=25&&x<195)dm_saver_preview();dm_preferences_save();}
}
const DmApp dm_settings_app={DM_API_VERSION,"settings","Impostazioni desktop",sizeof(Settings),open_settings,draw_settings,click_settings,0,0,0,"",0};
