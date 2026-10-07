#include "desktop_ui.h"
#include "preferences.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define INK DM_COLOR(24,38,55,255)
#define WHITE DM_COLOR(250,253,255,255)
#define LIGHT DM_COLOR(212,228,244,255)
static int show_date,popup,view_year,view_month,selected_day;
static long long offset_seconds;
static DmSystemInfo battery;
static uint64_t battery_update;
static time_t current_time(void) {
    return (time_t)((long long)dm_system_local_epoch()+offset_seconds);
}
int dm_days_in_month(int year,int month) {
    if(month<1||month>12)return 0;
    static const int days[]= {
        31,28,31,30,31,30,31,31,30,31,30,31
    };
    return days[month-1]+(month==2&&(year%4==0&&(year%100!=0||year%400==0)));
}
static void save_settings(void) {
#ifdef DESKTOP_PREVIEW
    if(getenv("DESKTOP_SELF_TEST"))return;
#endif
    char text[80];
    snprintf(text,sizeof(text),"%d\n%lld",show_date,offset_seconds);
    if(dm_fs_write("ux0:/data/desktop-mode/clock.txt",text,strlen(text),0)<0)dm_status("Opzioni orologio applicate, salvataggio fallito");
}
void dm_clock_initialize(void) {
    char text[80];
    if(dm_fs_read("ux0:/data/desktop-mode/clock.txt",text,sizeof(text))>=0) {
        int show;
        long long delta;
        if(sscanf(text,"%d\n%lld",&show,&delta)==2&&delta>=-2147483647LL&&delta<=2147483647LL) {
            show_date=show!=0;
            offset_seconds=delta;
        }
    }
    dm_system_info(&battery);
    battery_update=dm_clock_ms();
}
int dm_clock_set_local(const char*value) {
    int year,month,day,hour,minute,used=0;
    if(sscanf(value,"%d-%d-%d %d:%d%n",&year,&month,&day,&hour,&minute,&used)!=5||value[used]||year<1970||year>2037||month<1||month>12||day<1||day>dm_days_in_month(year,month)||hour<0||hour>23||minute<0||minute>59)return -1;
    struct tm t= {
        0
    };
    t.tm_year=year-1900;
    t.tm_mon=month-1;
    t.tm_mday=day;
    t.tm_hour=hour;
    t.tm_min=minute;
    t.tm_isdst=-1;
    time_t selected=mktime(&t);
    if(selected==(time_t)-1)return -1;
    offset_seconds=(long long)selected-dm_system_local_epoch();
    save_settings();
    return 0;
}
static void changed(const char*value,void*ctx) {
    (void)ctx;
    if(dm_clock_set_local(value)<0)dm_status("Data non valida: usa AAAA-MM-GG HH:MM (1970-2037)");
    else dm_status("Data e ora aggiornate per Desktop Mode");
}
void dm_clock_toggle_date(void) {
    show_date=!show_date;
    save_settings();
}
void dm_clock_dismiss(void) {
    popup=0;
}
int dm_clock_popup_active(void) {
    return popup;
}
static int inside(int x,int y,int a,int b,int w,int h) {
    return x>=a&&x<a+w&&y>=b&&y<b+h;
}
static void choose_month(int delta) {
    view_month+=delta;
    if(view_month<1) {
        view_month=12;
        view_year--;
    }
    if(view_month>12) {
        view_month=1;
        view_year++;
    }
    if(view_year<1970) {
        view_year=1970;
        view_month=1;
    }
    if(view_year>2037) {
        view_year=2037;
        view_month=12;
    }
    if(selected_day>dm_days_in_month(view_year,view_month))selected_day=dm_days_in_month(view_year,view_month);
}
int dm_clock_click(int x,int y) {
    if(inside(x,y,758,504,67,40)) {
        dm_launch("control",NULL);
        popup=0;
        return 1;
    }
    if(inside(x,y,825,504,135,40)) {
        popup=!popup;
        if(popup) {
            time_t time=current_time();
            struct tm*t=localtime(&time);
            view_year=t->tm_year+1900;
            view_month=t->tm_mon+1;
            selected_day=t->tm_mday;
        }
        return 1;
    }
    if(!popup)return 0;
    if(!inside(x,y,630,125,325,379)) {
        popup=0;
        return 0;
    }
    if(inside(x,y,644,163,35,28)) {
        choose_month(-1);
        return 1;
    }
    if(inside(x,y,905,163,35,28)) {
        choose_month(1);
        return 1;
    }
    if(inside(x,y,645,376,294,27)) {
        dm_clock_toggle_date();
        return 1;
    }
    if(inside(x,y,645,410,294,28)) {
        time_t now=current_time();
        struct tm*t=localtime(&now);
        char text[80];
        snprintf(text,sizeof(text),"%04d-%02d-%02d %02d:%02d",view_year,view_month,selected_day,t->tm_hour,t->tm_min);
        dm_prompt("Data/ora Desktop Mode: AAAA-MM-GG HH:MM",text,changed,NULL);
        return 1;
    }
    if(inside(x,y,645,447,294,27)) {
        offset_seconds=0;
        save_settings();
        dm_status("Orologio sincronizzato con la console");
        return 1;
    }
    if(inside(x,y,645,225,294,140)) {
        struct tm t= {
            0
        };
        t.tm_year=view_year-1900;
        t.tm_mon=view_month-1;
        t.tm_mday=1;
        t.tm_isdst=-1;
        mktime(&t);
        int first=(t.tm_wday+6)%7;
        int day=(y-225)/23*7+(x-645)/42-first+1;
        if(day>=1&&day<=dm_days_in_month(view_year,view_month))selected_day=day;
        return 1;
    }
    return 1;
}
void dm_clock_draw_tray(void) {
    uint64_t now=dm_clock_ms();
    if(now-battery_update>2000) {
        dm_system_info(&battery);
        battery_update=now;
    }
    dm_rect(758,504,202,40,DM_COLOR(34,65,108,210));
    dm_rect(769,515,28,15,WHITE);
    dm_rect(797,520,3,5,WHITE);
    dm_rect(771,517,24,11,INK);
    int percent=battery.battery_percent;
    if(percent>=0) {
        if(percent>100)percent=100;
        unsigned color=percent<20?DM_COLOR(225,79,66,255):DM_COLOR(111,216,109,255);
        dm_rect(772,518,22*percent/100,9,color);
        if(battery.charging) {
            int phase=(now/130)%22;
            for(int n=0;n<4;n++) {
                int x=(phase+n)%22;
                dm_rect(772+x,518,1,9,DM_COLOR(219,255,198,255));
            }
        }
    }    else dm_text_center(783,528,"?",WHITE);
    char level[20];
    snprintf(level,sizeof(level),percent<0?"--":"%d%%",percent);
    dm_text_center(813,529,level,WHITE);
    time_t time=current_time();
    struct tm*t=localtime(&time);
    char clock[32];
    strftime(clock,sizeof(clock),"%H:%M",t);
    dm_text_center(895,show_date?520:530,clock,WHITE);
    if(show_date) {
        strftime(clock,sizeof(clock),"%d/%m/%Y",t);
        dm_text_center(895,539,clock,WHITE);
    }
}
void dm_clock_draw_popup(void) {
    if(!popup)return;
    static const char*months_it[]={"Gennaio","Febbraio","Marzo","Aprile","Maggio","Giugno","Luglio","Agosto","Settembre","Ottobre","Novembre","Dicembre"};
    static const char*months_en[]={"January","February","March","April","May","June","July","August","September","October","November","December"};
    static const char*months_es[]={"Enero","Febrero","Marzo","Abril","Mayo","Junio","Julio","Agosto","Septiembre","Octubre","Noviembre","Diciembre"};
    const char**months=dm_preferences.language==DM_LANG_EN?months_en:dm_preferences.language==DM_LANG_ES?months_es:months_it;
    dm_rect(627,122,331,382,DM_COLOR(15,34,60,245));
    dm_rect(630,125,325,379,DM_COLOR(236,245,253,255));
    dm_rect(630,125,325,30,DM_COLOR(36,76,138,255));
    dm_text(645,147,"Data e ora",WHITE);
    time_t stamp=current_time();
    char clock[32];
    strftime(clock,sizeof(clock),"%H:%M:%S",localtime(&stamp));
    dm_text_center(890,147,clock,WHITE);
    dm_rect(644,163,35,28,LIGHT);
    dm_text(655,184,"<",INK);
    dm_rect(905,163,35,28,LIGHT);
    dm_text(918,184,">",INK);
    char title[60];
    snprintf(title,sizeof(title),"%s %d",months[view_month-1],view_year);
    dm_text_center(793,184,title,INK);
    const char*days_it[]={"Lu","Ma","Me","Gi","Ve","Sa","Do"};
    const char*days_en[]={"Mo","Tu","We","Th","Fr","Sa","Su"};
    const char*days_es[]={"Lu","Ma","Mi","Ju","Vi","Sa","Do"};
    const char**days=dm_preferences.language==DM_LANG_EN?days_en:dm_preferences.language==DM_LANG_ES?days_es:days_it;
    for(int i=0;i<7;i++)dm_text_center(666+i*42,213,days[i],INK);
    struct tm first= {
        0
    };
    first.tm_year=view_year-1900;
    first.tm_mon=view_month-1;
    first.tm_mday=1;
    first.tm_isdst=-1;
    mktime(&first);
    int start=(first.tm_wday+6)%7;
    time_t now=current_time();
    struct tm current=*localtime(&now);
    for(int day=1;day<=dm_days_in_month(view_year,view_month);day++) {
        int cell=start+day-1,col=cell%7,row=cell/7,x=645+col*42,y=225+row*23;
        if(day==selected_day)dm_rect(x,y,40,22,DM_COLOR(102,170,226,255));
        else if(day==current.tm_mday&&view_month==current.tm_mon+1&&view_year==current.tm_year+1900)dm_rect(x,y,40,22,LIGHT);
        char label[16];
        snprintf(label,sizeof(label),"%d",day);
        dm_text_center(x+20,y+17,label,INK);
    }
    dm_rect(645,376,294,27,LIGHT);
    dm_text(654,395,show_date?"[x] Mostra data nella barra":"[ ] Mostra data nella barra",INK);
    dm_rect(645,410,294,28,LIGHT);
    dm_text(654,430,"Imposta data e ora desktop",INK);
    dm_rect(645,447,294,27,LIGHT);
    dm_text(654,466,"Usa ora della console",INK);
    dm_text(644,494,"Impostazione solo per Desktop Mode",INK);
}
