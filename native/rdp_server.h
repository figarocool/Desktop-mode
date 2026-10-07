#ifndef DM_RDP_SERVER_H
#define DM_RDP_SERVER_H
#include "desktop_api.h"
/* One TLS RDP session, fixed 960x544 desktop, no external OS session. */
typedef struct {int kind,x,y,left,right,key,modifiers;char text[8];} DmRemoteEvent;
int dm_rdp_start(const char*password,unsigned port);
void dm_rdp_stop(void);
void dm_rdp_tick(void);
int dm_rdp_enabled(void);
int dm_rdp_connected(void);
int dm_rdp_event(DmRemoteEvent*event);
void dm_rdp_capture(void);
const char*dm_rdp_status(void);
unsigned dm_rdp_port(void);
extern const DmApp dm_remote_app;
#endif
