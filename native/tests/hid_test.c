#include <assert.h>
#include <stdio.h>
#include "../input_devices.c"
static SceHidKeyboardReport keyboard;static SceHidMouseReport mouse;
static int key_available,mouse_available,connected=1;static uint64_t clock_time=1000;
uint64_t dm_clock_ms(void){return clock_time;}
int sceHidKeyboardEnumerate(int*handles,int capacity){assert(capacity==8);if(connected)handles[0]=11;return 0;}
int sceHidMouseEnumerate(int*handles,int capacity){assert(capacity==8);if(connected)handles[0]=21;return 0;}
int sceHidKeyboardPeek(SceUInt32 handle,SceHidKeyboardReport**reports,int count){assert(handle==11&&count==16);if(!key_available)return 0;memcpy(reports,&keyboard,sizeof(keyboard));key_available=0;return 1;}
int sceHidMouseRead(SceUInt32 handle,SceHidMouseReport**reports,int count){assert(handle==21&&count==16);if(!mouse_available)return 0;memcpy(reports,&mouse,sizeof(mouse));mouse_available=0;return 1;}
int main(void){
 char text[8];int key,x=50,y=50,held,right;
 mouse=(SceHidMouseReport){.buttons=3,.rel_x=12,.rel_y=-4,.timestamp=100};mouse_available=1;
 keyboard=(SceHidKeyboardReport){.keycodes={4},.timestamp=100};key_available=1;
 assert(dm_hid_input(text,sizeof(text),&key,&x,&y,&held,&right));assert(x==62&&y==46&&held==1&&right==1&&!strcmp(text,"a"));
 dm_hid_input(text,sizeof(text),&key,&x,&y,&held,&right);assert(x==62&&y==46&&held&&!right&&!text[0]);
 keyboard.modifiers[0]=1;keyboard.keycodes[0]=6;keyboard.timestamp++;key_available=1;dm_hid_input(text,sizeof(text),&key,&x,&y,&held,&right);assert(key==DM_KEY_COPY&&dm_hid_modifiers()==1);
 keyboard.modifiers[0]=2;keyboard.keycodes[0]=80;keyboard.timestamp++;key_available=1;dm_hid_input(text,sizeof(text),&key,&x,&y,&held,&right);assert(key==(DM_KEY_LEFT|DM_KEY_SHIFT)&&dm_hid_modifiers()==2);
 mouse.buttons=0;mouse.timestamp++;mouse_available=1;dm_hid_input(text,sizeof(text),&key,&x,&y,&held,&right);assert(!held);
 keyboard.modifiers[0]=0;keyboard.keycodes[0]=47;keyboard.timestamp++;key_available=1;dm_hid_input(text,sizeof(text),&key,&x,&y,&held,&right);assert(!strcmp(text,"è"));
 clock_time+=449;dm_hid_input(text,sizeof(text),&key,&x,&y,&held,&right);assert(!text[0]);clock_time++;dm_hid_input(text,sizeof(text),&key,&x,&y,&held,&right);assert(!strcmp(text,"è"));clock_time+=40;dm_hid_input(text,sizeof(text),&key,&x,&y,&held,&right);assert(!strcmp(text,"è"));
 keyboard.modifiers[0]=0x40;keyboard.keycodes[0]=51;keyboard.timestamp++;key_available=1;dm_hid_input(text,sizeof(text),&key,&x,&y,&held,&right);assert(!strcmp(text,"@"));
 keyboard.modifiers[0]=2;keyboard.keycodes[0]=32;keyboard.timestamp++;key_available=1;dm_hid_input(text,sizeof(text),&key,&x,&y,&held,&right);assert(!strcmp(text,"£"));
 keyboard.keycodes[0]=0;keyboard.timestamp++;key_available=1;dm_hid_input(text,sizeof(text),&key,&x,&y,&held,&right);clock_time+=1000;dm_hid_input(text,sizeof(text),&key,&x,&y,&held,&right);assert(!text[0]&&!key);
 connected=0;clock_time+=1100;dm_hid_input(text,sizeof(text),&key,&x,&y,&held,&right);assert(kc==0&&mc==0&&!held&&dm_hid_modifiers()==0);
 puts("PASS: dense HID report ABI, enumeration status/handles, mouse motion/buttons, Italian UTF-8/AltGr/Ctrl/Shift, repeat/release and disconnect");
}
