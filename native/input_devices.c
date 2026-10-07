#include "device_ui.h"
#ifndef DESKTOP_PREVIEW
#include <psp2/hid.h>
#include <string.h>
static uint64_t key_stamp[8],mouse_stamp[8];static unsigned char old_keys[8][6];static int key_handles[8],mouse_handles[8],kc,mc,last_handles[8],last_mice[8],mouse_buttons[8];static uint64_t enumerated;
static unsigned char modifiers,device_modifiers[8],device_caps[8];
static uint64_t repeat_at[8];
static int repeat_code[8];
/* USB HID usages translated to the Italian keyboard layout. */
static const char* letter(int code,int shift,int altgr){
 static char ascii[2];
 if(altgr){switch(code){case 47:return "[";case 48:return "]";case 51:return "@";case 52:return "#";case 46:return "~";case 8:return "€";}return "";}
 if(code>=4&&code<=29){ascii[0]=(shift?'A':'a')+code-4;ascii[1]=0;return ascii;}
 if(code>=30&&code<=39){static const char* shifted[]={"!","\"","£","$","%","&","/","(",")","="};ascii[0]=code==39?'0':'1'+code-30;ascii[1]=0;return shift?shifted[code-30]:ascii;}
 if(code>=89&&code<=98){ascii[0]=code==98?'0':'1'+code-89;ascii[1]=0;return ascii;}
 switch(code){case 44:return " ";case 45:return shift?"?":"'";case 46:return shift?"^":"ì";case 47:return shift?"é":"è";case 48:return shift?"*":"+";case 49:return shift?"§":"ù";case 51:return shift?"ç":"ò";case 52:return shift?"°":"à";case 53:return shift?"|":"\\";case 54:return shift?";":",";case 55:return shift?":":".";case 56:return shift?"_":"-";case 100:return shift?">":"<";case 84:return "/";case 85:return "*";case 86:return "-";case 87:return "+";case 99:return ".";}return "";
}
static void emit_key(int code,unsigned char mods,unsigned char caps,char*text,size_t cap,int*key){
 int shift=!!(mods&0x22),altgr=!!(mods&0x40),ctrl=(mods&0x11)&&!altgr;
 const char*ch=letter(code,code>=4&&code<=29?shift^!!(caps&2):shift,altgr);
 if(ctrl){switch(code){case 4:*key=DM_KEY_SELECT_ALL;break;case 6:*key=DM_KEY_COPY;break;case 25:*key=DM_KEY_PASTE;break;case 27:*key=DM_KEY_CUT;break;case 22:*key=DM_KEY_SAVE;break;}}
 else if(*ch){size_t length=strlen(text),n=strlen(ch);if(length+n<cap)memcpy(text+length,ch,n+1);}
 else{switch(code){case 40:case 88:*key=DM_KEY_ENTER;break;case 42:*key=DM_KEY_BACKSPACE;break;case 76:*key=DM_KEY_DELETE;break;case 74:*key=DM_KEY_HOME;break;case 77:*key=DM_KEY_END;break;case 79:*key=DM_KEY_RIGHT;break;case 80:*key=DM_KEY_LEFT;break;case 81:*key=DM_KEY_DOWN;break;case 82:*key=DM_KEY_UP;break;}}
 if(shift&&((*key>=DM_KEY_LEFT&&*key<=DM_KEY_DOWN)||*key==DM_KEY_HOME||*key==DM_KEY_END))*key|=DM_KEY_SHIFT;
}
int dm_hid_input(char*text,size_t cap,int*key,int*x,int*y,int*held,int*right){
 text[0]=0;*key=0;*right=0;uint64_t now=dm_clock_ms();if(now-enumerated>=1000||!enumerated){enumerated=now;memset(key_handles,0,sizeof(key_handles));memset(mouse_handles,0,sizeof(mouse_handles));int kr=sceHidKeyboardEnumerate(key_handles,8),mr=sceHidMouseEnumerate(mouse_handles,8);kc=mc=0;if(kr>=0)for(int n=0;n<8;n++)if(key_handles[n]>0)key_handles[kc++]=key_handles[n];if(mr>=0)for(int n=0;n<8;n++)if(mouse_handles[n]>0)mouse_handles[mc++]=mouse_handles[n];if(!kc)modifiers=0;for(int i=0;i<8;i++){if(i>=kc||key_handles[i]!=last_handles[i]){key_stamp[i]=0;repeat_code[i]=0;repeat_at[i]=0;device_modifiers[i]=0;memset(old_keys[i],0,6);last_handles[i]=i<kc?key_handles[i]:0;}if(i>=mc||mouse_handles[i]!=last_mice[i]){mouse_stamp[i]=0;mouse_buttons[i]=0;last_mice[i]=i<mc?mouse_handles[i]:0;}}}
 int changed=0;*held=0;
 for(int i=0;i<mc;i++){SceHidMouseReport r[16];int count=sceHidMouseRead(mouse_handles[i],(SceHidMouseReport**)r,16);for(int j=0;j<count&&j<16;j++)if(r[j].timestamp!=mouse_stamp[i]){mouse_stamp[i]=r[j].timestamp;*x+=r[j].rel_x;*y+=r[j].rel_y;*right|=(r[j].buttons&2)&&!(mouse_buttons[i]&2);mouse_buttons[i]=r[j].buttons;if(r[j].wheel)*key=r[j].wheel>0?DM_KEY_SCROLL_UP:DM_KEY_SCROLL_DOWN;changed=1;}*held|=mouse_buttons[i]&1;}
 for(int i=0;i<kc;i++){
  SceHidKeyboardReport r[16];int count=sceHidKeyboardPeek(key_handles[i],(SceHidKeyboardReport**)r,16);
  for(int j=0;j<count&&j<16;j++)if(r[j].timestamp!=key_stamp[i]){
   key_stamp[i]=r[j].timestamp;device_modifiers[i]=r[j].modifiers[0];device_caps[i]=r[j].modifiers[1];
   int repeat_held=0;
   for(int k=0;k<6;k++){
    int code=r[j].keycodes[k],old=0;if(code==repeat_code[i])repeat_held=1;
    for(int q=0;q<6;q++)if(old_keys[i][q]==code)old=1;
    if(code<4||old)continue;
    emit_key(code,device_modifiers[i],device_caps[i],text,cap,key);
    repeat_code[i]=code;repeat_at[i]=now+450;repeat_held=1;changed=1;
   }
   if(!repeat_held)repeat_code[i]=0;
   memcpy(old_keys[i],r[j].keycodes,6);
  }
  if(repeat_code[i]&&now>=repeat_at[i]){
   /* Editing/navigation repeats; Ctrl shortcuts fire only on the initial press. */
   if(!(device_modifiers[i]&0x11)||(device_modifiers[i]&0x40)){
    emit_key(repeat_code[i],device_modifiers[i],device_caps[i],text,cap,key);changed=1;
   }
   repeat_at[i]=now+40;
  }
 }
 modifiers=0;for(int i=0;i<kc;i++)modifiers|=device_modifiers[i];
 return changed;
}
int dm_hid_modifiers(void){return(modifiers&0x11?1:0)|(modifiers&0x22?2:0);}
#endif
