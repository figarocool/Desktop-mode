#ifndef DESKTOP_MODE_TEST_SCE_STAT_H
#define DESKTOP_MODE_TEST_SCE_STAT_H
#include <stdint.h>
#include <sys/stat.h>
typedef struct { int64_t st_size; unsigned st_mode; } SceIoStat;
#define SCE_S_ISDIR(mode) S_ISDIR(mode)
int sceIoGetstat(const char *path, SceIoStat *result);
#endif
