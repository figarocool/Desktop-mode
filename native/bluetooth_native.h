#ifndef DM_BLUETOOTH_NATIVE_H
#define DM_BLUETOOTH_NATIVE_H
#include <stdint.h>
#define DM_BT_MAX 32
typedef struct {uint64_t mac;char name[128];int connection,registered;} DmBtDevice;
typedef struct {DmBtDevice devices[DM_BT_MAX];int count,scanning,ready,error;uint64_t auth_mac;int auth_kind;} DmBtState;
const DmBtState*dm_bt_state(void);
int dm_bt_init(void);
int dm_bt_enabled(void);
int dm_bt_set_enabled(int enabled);
void dm_bt_poll(void);
void dm_bt_shutdown(void);
int dm_bt_scan(void);
int dm_bt_connect(int);
int dm_bt_disconnect(int);
int dm_bt_refresh_registered(void);
int dm_bt_forget(uint64_t);
int dm_bt_pin(const char*);
int dm_bt_confirm(int);
#endif
