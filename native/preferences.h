#ifndef DM_PREFERENCES_H
#define DM_PREFERENCES_H
#include "desktop_api.h"
typedef struct {int icon_size,font_percent,language,saver_enabled,saver_seconds,cpu_mhz,gpu_mhz;char saver[64];} DmPreferences;
extern DmPreferences dm_preferences;
void dm_preferences_init(void);
int dm_preferences_save(void);
int dm_language_set(int);
int dm_appearance_set(int,int);
void dm_appearance_reset(void);
int dm_clock_set(int,int);
int dm_clock_restore(void);
extern const DmApp dm_settings_app;
const char*dm_ui_translate(const char*);
void dm_saver_tick(unsigned,int,int);
void dm_saver_draw(void);
int dm_saver_active(void);
int dm_saver_wake(void);
void dm_saver_preview(void);
int dm_saver_count(void);
const DmScreenSaver*dm_saver_at(int);
void dm_scan_savers(void);
#endif
