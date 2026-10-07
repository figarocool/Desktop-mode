#ifndef DM_APP_MANAGER_H
#define DM_APP_MANAGER_H
#include "desktop_api.h"
typedef struct {char path[DM_PATH_MAX];const DmApp*app;int installed,bundled;} DmInstalledApp;
int dm_registered_count(void);
const DmApp*dm_registered_app(int index);
int dm_unregister_app(const char*id);
int dm_plugin_list(DmInstalledApp*out,int capacity);
int dm_plugin_uninstall(const char*id);
int dm_plugin_reinstall(const char*id);
int dm_plugin_install(const char*path);
int dm_plugin_bundle_removed(const char*id);
const char *dm_plugin_diagnostic(void);
#ifdef DESKTOP_PREVIEW
void dm_plugin_test_settings(const char*path);
#endif
extern const DmApp dm_app_manager_app;
#endif
