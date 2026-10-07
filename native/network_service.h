#ifndef DM_NETWORK_SERVICE_H
#define DM_NETWORK_SERVICE_H
#include "desktop_api.h"
#include "app-sdk/network_wire.h"

int dm_network_service_open(DmWindow *owner);
void dm_network_service_close(DmWindow *owner);
void dm_network_service_tick(DmWindow *owner, unsigned elapsed_ms);
int dm_network_service_action(DmWindow *owner, int action);
int dm_network_service_select(DmWindow *owner, int index, int activate);
int dm_network_service_info(DmWindow *owner, DmNetworkInfo *info);
int dm_network_service_row(DmWindow *owner, int index, DmNetworkRow *row);
#endif
