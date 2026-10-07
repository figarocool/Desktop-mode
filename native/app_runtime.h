#ifndef DM_APP_RUNTIME_H
#define DM_APP_RUNTIME_H
#include "desktop_api.h"
extern DmWindow*dm_current_window;
void dm_runtime_draw_tray(void);
int dm_runtime_tray_click(int,int);
void dm_runtime_closed(DmWindow*);
void dm_memory_image(void*,uint64_t);
void dm_memory_image_free(void*);
#endif
