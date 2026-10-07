#ifndef DM_NETWORK_SETTINGS_H
#define DM_NETWORK_SETTINGS_H
#include "desktop_api.h"
typedef struct {
    int state,wifi,signal,mtu,error,preview,wifi_radio;
    char name[65],ssid[33],ip[16],mask[16],gateway[16],dns[2][16],mac[18],proxy[128];
} DmNetworkAdapter;
#define DM_WIFI_MAX_ACCESS_POINTS 16
typedef struct {char ssid[33];int security,signal;} DmWifiAccessPoint;
void dm_network_adapter(DmNetworkAdapter*);
int dm_network_configure(int manual_wifi);
int dm_wifi_radio_enabled(void);
int dm_wifi_radio_set(int enabled);
int dm_wifi_scan_start(void);
int dm_wifi_scan_poll(DmWifiAccessPoint*,int capacity);
int dm_wifi_connect(const char*ssid,const char*password);
void dm_network_tray_draw(void);
int dm_network_tray_click(int,int);
extern const DmApp dm_network_settings_app;
#endif
