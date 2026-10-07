#ifndef DM_DROP_TRANSFER_H
#define DM_DROP_TRANSFER_H
#include "desktop_api.h"
int dm_drop_begin(const char(*paths)[DM_PATH_MAX],int count,const char*directory);
int dm_drop_copy_begin(const char(*paths)[DM_PATH_MAX],int count,const char*directory,int delete_sources_on_success);
void dm_drop_draw(void);
int dm_drop_click(int,int);
int dm_drop_active(void);
void dm_drop_cancel(void);
#endif
