#ifndef MOCK_CALLBACK_H
#define MOCK_CALLBACK_H
typedef int (*SceKernelCallbackFunction)(int,int,int,void*);
int sceKernelCreateCallback(const char*,unsigned,SceKernelCallbackFunction,void*);
int sceKernelDeleteCallback(int);
int sceKernelCheckCallback(void);
#endif
