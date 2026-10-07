#ifndef MOCK_HID_H
#define MOCK_HID_H
#include <stdint.h>
typedef uint32_t SceUInt32;
typedef struct {uint8_t reserved,modifiers[2],keycodes[6],reserved2[7];uint64_t timestamp;} SceHidKeyboardReport;
typedef struct {uint8_t buttons,reserved;int16_t rel_x,rel_y;int8_t wheel,tilt;uint64_t timestamp;} SceHidMouseReport;
int sceHidKeyboardEnumerate(int*,int);int sceHidMouseEnumerate(int*,int);
int sceHidKeyboardPeek(SceUInt32,SceHidKeyboardReport**,int);int sceHidMouseRead(SceUInt32,SceHidMouseReport**,int);
#endif
