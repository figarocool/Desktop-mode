#include "../app-sdk/desktop_plugin.h"
#include <stdio.h>
typedef struct {
    int count;
}
Counter;
static void draw(DmWindow*w) {
    char s[64];
    snprintf(s,sizeof(s),dm_localize("Contatore: %d","Counter: %d","Contador: %d"),((Counter*)w->state)->count);
    dm_text(w->x+30,w->y+100,s,DM_COLOR(25,40,60,255));
    dm_rect(w->x+30,w->y+135,210,42,DM_COLOR(190,212,236,255));
    dm_text(w->x+45,w->y+163,"Aggiungi 1",DM_COLOR(25,40,60,255));
    dm_text(w->x+30,w->y+235,"App C registrata tramite Desktop API v1",DM_COLOR(25,40,60,255));
}
static void click(DmWindow*w,int x,int y) {
    if(x>=30&&x<240&&y>=135&&y<177)((Counter*)w->state)->count++;
}
const DmApp dm_counter_app= {
    DM_API_VERSION,"counter","Contatore - esempio API",sizeof(Counter),0,draw,click,0,0,0,"",0
};
DM_EXPORT_APP(dm_counter_app)
