#ifndef DM_WASM_SANDBOX_H
#define DM_WASM_SANDBOX_H
#include "desktop_api.h"
#include <stddef.h>
#include <stdint.h>

enum { DM_WASM_OK = 0, DM_WASM_TRAP = 1, DM_WASM_INVALID = -1, DM_WASM_FUEL = -2 };
int dm_wasm_run_probe(const uint8_t *module, size_t size, int32_t *result);
int dm_wasm_run_builtin_tests(int *add_status, int32_t *value, int *trap_status);
extern const DmApp dm_wasm_sandbox_app;

#endif
