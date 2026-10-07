#include "../app-sdk/desktop_plugin.h"
#include <stdio.h>
typedef struct {char argument[DM_PATH_MAX];unsigned elapsed;} State;
static void open_app(DmWindow*w,const char*arg){snprintf(((State*)w->state)->argument,DM_PATH_MAX,"%s",arg?arg:"");}
static void draw(DmWindow*w){dm_text(w->x+10,w->y+60,"Plugin di test",DM_COLOR(0,0,0,255));}
static void tick(DmWindow*w,unsigned dt){((State*)w->state)->elapsed+=dt;}
const DmApp fixture={DM_API_VERSION,"fixture","Fixture",sizeof(State),open_app,draw,0,0,0,0,".fixture",tick};
DM_EXPORT_APP(fixture)
