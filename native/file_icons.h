#ifndef DM_FILE_ICONS_H
#define DM_FILE_ICONS_H
#include "desktop_api.h"
int dm_file_icon(const char*path,int x,int y,int size);
void dm_file_icons_clear(void);
/* Returns a small RGBA thumbnail, without allocating a full image texture. */
int dm_thumbnail_pixels(const char*path,uint32_t*pixels,unsigned*width,unsigned*height);
#endif
