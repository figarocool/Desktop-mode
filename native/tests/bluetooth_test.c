#include <assert.h>
#include <stdio.h>
#include "../bluetooth_native.c"
static SceKernelCallbackFunction callback_fn;
static BtEvent queue[20];static int queued,at_event,starts,stops,connects,disconnects,pins,confirms,registration_error,radio=9,api_error;
int sceKernelCreateCallback(const char*n,unsigned attr,SceKernelCallbackFunction f,void*u){(void)n;(void)attr;(void)u;callback_fn=f;return 10;}
int sceKernelDeleteCallback(int id){assert(id==10);return 0;}
int sceKernelCheckCallback(void){if(at_event<queued)callback_fn(0,0,0,NULL);return 0;}
int sceBtGetConfiguration(void){return radio;}
int sceBtSetConfiguration(int enabled){assert(enabled==0||enabled==1);radio=enabled;return api_error;}
int sceBtGetRegisteredInfo(int slot,int flags,BtRegisteredInfo*info,unsigned size){(void)slot;(void)flags;(void)info;(void)size;return -1;}
int sceBtDeleteRegisteredInfo(uint64_t mac){(void)mac;return 0;}
int sceBtStartInquiry(void){starts++;return api_error;}
int sceBtStopInquiry(void){stops++;return api_error;}
int sceBtStartConnect(uint64_t mac){assert(mac==0x123456789abcULL);connects++;return api_error;}
int sceBtStartDisconnect(uint64_t mac){assert(mac==0x123456789abcULL);disconnects++;return api_error;}
int sceBtGetConnectingInfo(uint64_t mac){(void)mac;return -1;}
int sceBtGetDeviceName(uint64_t mac,char*name){(void)mac;snprintf(name,0x79,"Tastiera test");return 0;}
int sceBtRegisterCallback(int id,int unused,int f1,int f2){assert(id==10&&!unused&&f1==-1&&f2==-1);return registration_error;}
int sceBtUnregisterCallback(int id){assert(id==10);return 0;}
int sceBtReadEvent(BtEvent*event,int count){assert(count==1);if(at_event==queued)return 0;*event=queue[at_event++];return 1;}
int sceBtReplyPinCode(uint64_t mac,const unsigned char*pin,unsigned length){assert(mac==0x123456789abcULL&&length==4&&!memcmp(pin,"1234",4));pins++;return api_error;}
int sceBtReplyUserConfirmation(uint64_t mac,int yes){assert(mac==0x123456789abcULL&&yes==0);confirms++;return api_error;}
static void event(unsigned id){queue[queued++]=(BtEvent){.id=id,.mac=0x123456789abcULL};dm_bt_poll();}
int main(void){
 registration_error=-42;assert(dm_bt_init()==-42&&!state.ready&&callback==-1);registration_error=0;assert(!dm_bt_init()&&state.ready);radio=0;assert(dm_bt_enabled()==0&&dm_bt_scan()<0&&!state.scanning);radio=9;assert(dm_bt_enabled()==1&&!dm_bt_scan()&&state.scanning&&starts==1);event(1);assert(state.count==1&&!strcmp(state.devices[0].name,"Tastiera test"));assert(!dm_bt_connect(0)&&stops==1&&connects==0);event(2);assert(!state.scanning&&connects==1);event(3);assert(state.auth_kind==1);assert(dm_bt_pin("")<0&&state.auth_kind==1);assert(!dm_bt_pin("1234")&&pins==1&&!state.auth_kind);event(4);assert(state.auth_kind==2);assert(!dm_bt_confirm(0)&&confirms==1);event(5);assert(state.devices[0].connection==5);assert(!dm_bt_disconnect(0)&&disconnects==1);event(6);assert(state.devices[0].connection==1);api_error=-11;assert(dm_bt_connect(0)==-11&&state.error==-11);assert(dm_bt_connect(-1)<0);api_error=0;assert(!dm_bt_set_enabled(0)&&radio==0&&callback==-1);assert(!dm_bt_set_enabled(1)&&radio==1&&callback==10);dm_bt_shutdown();assert(callback==-1&&!state.ready);puts("PASS: native SceBt callback ABI, radio on/off, discovery, stop-before-connect, PIN/confirmation, disconnect and cleanup (mock backend)");
}
