#ifndef DEVICE_UI_H
#define DEVICE_UI_H
#include "desktop_api.h"
int dm_device_tray_click(int,int);
void dm_device_tray_draw(void);
void dm_device_popup_draw(void);
void dm_device_tick(int,int,int);
int dm_device_popup_active(void);
void dm_device_dismiss(void);
int dm_volume_get(void);
int dm_volume_set(int);
void dm_hid_counts(int*,int*);
int dm_bluetooth_settings(void);
#ifndef DESKTOP_PREVIEW
int dm_hid_input(char*,size_t,int*,int*,int*,int*,int*);
#endif
#endif
