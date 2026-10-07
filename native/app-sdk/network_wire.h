#ifndef DM_NETWORK_WIRE_H
#define DM_NETWORK_WIRE_H
#include <stdint.h>
typedef struct {
    char server[64], share[256], path[1024], message[256];
    uint32_t count, offset, selected, mode;
    uint32_t busy, scanning, connected, download_active, upload_active;
    uint64_t tested, scan_total, downloaded, download_size, uploaded, upload_size;
} DmNetworkInfo;
typedef struct { char name[256]; uint32_t directory, smb; uint64_t size; } DmNetworkRow;
#endif
