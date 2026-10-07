#ifndef DESKTOP_MODE_TEST_SCE_FCNTL_H
#define DESKTOP_MODE_TEST_SCE_FCNTL_H
#include <fcntl.h>
#define SCE_O_RDONLY O_RDONLY
int sceIoOpen(const char *path, int flags, int mode);
int sceIoRead(int fd, void *buffer, unsigned size);
int sceIoClose(int fd);
#endif
