#ifndef DM_RENAME_H
#define DM_RENAME_H
#include "desktop_api.h"
int dm_rename_dialog(const char*path);
void dm_path_renamed(char*path,size_t capacity,const char*source,const char*destination);
#endif
