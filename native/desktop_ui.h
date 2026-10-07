#ifndef DESKTOP_UI_H
#define DESKTOP_UI_H
#include "desktop_api.h"
#include <time.h>
void dm_clock_initialize(void);
void dm_file_dialog_refresh(void);
void dm_clock_draw_tray(void);
void dm_clock_draw_popup(void);
int dm_clock_click(int x,int y);
void dm_clock_dismiss(void);
time_t dm_system_local_epoch(void);
int dm_days_in_month(int year,int month);
int dm_clock_set_local(const char*value);
int dm_clock_popup_active(void);
void dm_clock_toggle_date(void);
#endif
