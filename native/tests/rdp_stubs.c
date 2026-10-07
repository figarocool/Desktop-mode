/* Isolated protocol harness: no preview files and no sockets. */
#include "../desktop_api.h"
#include <time.h>
#include <string.h>
uint64_t dm_clock_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
void dm_system_info(DmSystemInfo*info){memset(info,0,sizeof(*info));}
int dm_fs_read(const char*p,char*b,size_t n){(void)p;(void)b;(void)n;return -1;}
int dm_fs_write(const char*p,const void*b,size_t n,int exclusive){(void)p;(void)b;(void)n;(void)exclusive;return -1;}
int desktop_capture_rgba(void*b,int stride){(void)b;(void)stride;return -1;}
