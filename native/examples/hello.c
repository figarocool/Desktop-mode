#include "desktop_plugin.h"
#include <stdio.h>
typedef struct {char opened_path[DM_PATH_MAX];unsigned elapsed;} Hello;
static void open_app(DmWindow*w,const char*argument){Hello*s=w->state;snprintf(s->opened_path,sizeof(s->opened_path),"%s",argument?argument:"Nessun file");}
static void draw(DmWindow*w){Hello*s=w->state;dm_text(w->x+25,w->y+90,"App esterna: nessuna modifica al desktop",DM_COLOR(24,38,55,255));dm_text(w->x+25,w->y+135,s->opened_path,DM_COLOR(24,38,55,255));char line[80];snprintf(line,sizeof(line),"Tempo attivo (anche minimizzata): %u ms",s->elapsed);dm_text(w->x+25,w->y+180,line,DM_COLOR(24,38,55,255));}
static void tick(DmWindow*w,unsigned elapsed){((Hello*)w->state)->elapsed+=elapsed;}
const DmApp hello_app={DM_API_VERSION,"hello","Hello - app esterna",sizeof(Hello),open_app,draw,0,0,0,0,".hello",tick};
DM_EXPORT_APP(hello_app)
