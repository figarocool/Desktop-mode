#ifndef DM_WASM_APP_RUNTIME_H
#define DM_WASM_APP_RUNTIME_H
#include <stddef.h>
#include <stdint.h>
int dm_wasm_register_package(const char *id, const char *title,
                             const uint8_t *bytes, size_t size);
#endif
