#ifndef MULTI_FILE_H
#define MULTI_FILE_H
#include "desktop_api.h"
int dm_multi_copy(const char (*paths)[DM_PATH_MAX],int count,const char*directory);
int dm_multi_trash(const char (*paths)[DM_PATH_MAX],int count);
#endif
