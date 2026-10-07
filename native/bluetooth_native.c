/* Userland SceBt imports: VitaSDK db/360/SceBt.yml; structs match 0x10/0x100 firmware ABI. */
#include "bluetooth_native.h"
#include <stdio.h>
#include <string.h>
static DmBtState state;
const DmBtState*dm_bt_state(void){return &state;}
#ifdef DESKTOP_PREVIEW
int dm_bt_init(void){state.error=-1;return -1;}
int dm_bt_enabled(void){return -1;}
int dm_bt_set_enabled(int enabled){(void)enabled;return -1;}
void dm_bt_poll(void){}
void dm_bt_shutdown(void){}
int dm_bt_scan(void){return -1;}
int dm_bt_connect(int n){(void)n;return -1;}
int dm_bt_disconnect(int n){(void)n;return -1;}
int dm_bt_refresh_registered(void){return -1;}
int dm_bt_forget(uint64_t mac){(void)mac;return -1;}
int dm_bt_pin(const char*p){(void)p;return -1;}
int dm_bt_confirm(int yes){(void)yes;return -1;}
#else
#include <psp2/kernel/threadmgr/callback.h>
typedef struct {uint8_t id,unknown1;uint16_t unknown2;uint32_t unknown4;uint64_t mac;} BtEvent;
_Static_assert(sizeof(BtEvent)==16,"SceBt event ABI");
typedef struct {uint64_t mac;uint32_t bt_class,bt_profile,unk10;uint16_t vid,pid;uint32_t unk18,unk1c;char name[128];uint8_t reserved[96];} BtRegisteredInfo;
_Static_assert(sizeof(BtRegisteredInfo)==0x100,"SceBt registered-info ABI");
extern int sceBtGetConfiguration(void);
extern int sceBtSetConfiguration(int);
extern int sceBtStartInquiry(void);
extern int sceBtStopInquiry(void);
extern int sceBtStartConnect(uint64_t);
extern int sceBtStartDisconnect(uint64_t);
extern int sceBtGetConnectingInfo(uint64_t);
extern int sceBtGetDeviceName(uint64_t,char*);
extern int sceBtGetRegisteredInfo(int,int,BtRegisteredInfo*,unsigned);
extern int sceBtDeleteRegisteredInfo(uint64_t);
extern int sceBtRegisterCallback(int,int,int,int);
extern int sceBtUnregisterCallback(int);
extern int sceBtReadEvent(BtEvent*,int);
extern int sceBtReplyPinCode(uint64_t,const unsigned char*,unsigned);
extern int sceBtReplyUserConfirmation(uint64_t,int);
static int callback=-1,pending=-1;
static uint64_t connecting;
static int result(int r){state.error=r<0?r:0;return r;}
int dm_bt_enabled(void){int r=sceBtGetConfiguration();if(r<0)return result(r);state.error=0;return (r&9)!=0;}
int dm_bt_set_enabled(int enabled){
 if(enabled!=0&&enabled!=1)return result(-1);
 if(state.scanning){int r=sceBtStopInquiry();if(r<0)return result(r);state.scanning=0;pending=-1;}
 int had_callback=callback>=0;
 if(!enabled&&had_callback)dm_bt_shutdown();
 int r=sceBtSetConfiguration(enabled?1:0);
 if(r<0){if(had_callback&&callback<0)dm_bt_init();return result(r);}
 else if(enabled&&callback<0){r=dm_bt_init();if(r<0)return result(r);}
 return result(0);
}
static int find(uint64_t mac){for(int i=0;i<state.count;i++)if(state.devices[i].mac==mac)return i;if(state.count==DM_BT_MAX)return -1;int i=state.count++;state.devices[i].mac=mac;snprintf(state.devices[i].name,128,"%012llX",(unsigned long long)(mac&0xffffffffffffULL));return i;}
int dm_bt_refresh_registered(void){
 for(int i=0;i<state.count;i++)state.devices[i].registered=0;
 int found=0;
 for(int slot=0;slot<DM_BT_MAX;slot++){
  BtRegisteredInfo info={0};int r=sceBtGetRegisteredInfo(slot,0,&info,sizeof(info));
  if(r<0){if(slot==0)state.error=r;break;}
  if(!info.mac)continue;
  int i=find(info.mac);if(i<0)continue;
  state.devices[i].registered=1;state.devices[i].connection=sceBtGetConnectingInfo(info.mac);
  if(info.name[0]){info.name[sizeof(info.name)-1]=0;snprintf(state.devices[i].name,sizeof(state.devices[i].name),"%s",info.name);}
  found++;
 }
 if(found||!state.error)state.error=0;
 return found;
}
int dm_bt_forget(uint64_t mac){if(!mac)return result(-1);int r=sceBtDeleteRegisteredInfo(mac);if(r<0)return result(r);for(int i=0;i<state.count;i++)if(state.devices[i].mac==mac)state.devices[i].registered=0;return result(r);}
static void drain(void){
 for(int iteration=0;iteration<64;iteration++){
  BtEvent event={0};int r=sceBtReadEvent(&event,1);if(r<=0){if(r<0)state.error=r;break;}
  int i=-1;if(event.mac)i=find(event.mac);
  if(i>=0){char name[128]={0};if(sceBtGetDeviceName(event.mac,name)>=0&&name[0]){name[127]=0;snprintf(state.devices[i].name,128,"%s",name);}}
  switch(event.id){
   case 1:break; /* inquiry result */
   case 2:state.scanning=0;if(pending>=0){int n=pending;pending=-1;connecting=state.devices[n].mac;result(sceBtStartConnect(connecting));}break;
   case 3:if(event.mac==connecting){state.auth_mac=event.mac;state.auth_kind=1;}break; /* PIN event mapping still requires hardware validation. */
   case 4:if(event.mac==connecting){state.auth_mac=event.mac;state.auth_kind=2;}break;
   case 5:if(i>=0)state.devices[i].connection=5;state.auth_kind=0;break;
   case 6:if(i>=0)state.devices[i].connection=1;state.auth_kind=0;break;
  }
 }
}
static int notified(int id,int count,int arg,void*context){(void)id;(void)count;(void)arg;(void)context;drain();return 0;}
int dm_bt_init(void){
 if(callback>=0)return 0;
 callback=sceKernelCreateCallback("DesktopModeBluetooth",0,notified,NULL);if(callback<0)return result(callback);
 int r=sceBtRegisterCallback(callback,0,-1,-1);if(r<0){sceKernelDeleteCallback(callback);callback=-1;return result(r);}state.ready=1;return result(0);
}
void dm_bt_poll(void){if(callback<0)return;sceKernelCheckCallback();for(int i=0;i<state.count;i++){int r=sceBtGetConnectingInfo(state.devices[i].mac);if(r>=0)state.devices[i].connection=r;}}
void dm_bt_shutdown(void){if(callback<0)return;if(state.scanning)sceBtStopInquiry();sceBtUnregisterCallback(callback);sceKernelDeleteCallback(callback);callback=-1;pending=-1;connecting=0;state.ready=state.scanning=state.auth_kind=0;}
int dm_bt_scan(void){if(dm_bt_init()<0)return state.error;int r=sceBtGetConfiguration();if(r<=0)return result(r<0?r:-2);if(state.scanning)return 0;r=sceBtStartInquiry();if(r>=0)state.scanning=1;return result(r);}
int dm_bt_connect(int n){if(n<0||n>=state.count)return result(-1);if(state.scanning){pending=n;int r=sceBtStopInquiry();if(r<0)pending=-1;return result(r);}connecting=state.devices[n].mac;return result(sceBtStartConnect(connecting));}
int dm_bt_disconnect(int n){if(n<0||n>=state.count)return result(-1);return result(sceBtStartDisconnect(state.devices[n].mac));}
int dm_bt_pin(const char*p){size_t n=strlen(p);if(!state.auth_kind||n<1||n>16)return result(-1);int r=sceBtReplyPinCode(state.auth_mac,(const unsigned char*)p,n);if(r>=0)state.auth_kind=0;return result(r);}
int dm_bt_confirm(int yes){if(state.auth_kind!=2)return result(-1);int r=sceBtReplyUserConfirmation(state.auth_mac,yes!=0);if(r>=0)state.auth_kind=0;return result(r);}
#endif
