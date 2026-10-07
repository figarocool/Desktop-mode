/* Direct parser checks and randomized malformed PDUs under sanitizers. */
#define DESKTOP_PREVIEW
#define DM_RDP_TEST
#include "../rdp_server.c"
#include <assert.h>
static unsigned random_state=0x1205;
static unsigned random_value(void){random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}
static void reset(int phase){if(!s){s=calloc(1,sizeof(*s));assert(s);}memset(s,0,sizeof(*s));s->fd=123456;s->listener=-1;s->phase=phase;strcpy(s->password,"wire-test-password");}
int main(void){
 unsigned char data[2048];
 reset(HELLO);
 unsigned char hello[19]={3,0,0,19,14,0xe0,0,0,0,0,0,1,0,8,0,1,0,0,0};
 for(unsigned n=0;n<19;n++)assert(process_packet(hello,n)<0);
 assert(process_packet(hello,19)==0&&s->phase==TLS&&!s->authenticated);
 reset(CHANNELS);s->attached=1;
 unsigned char join[12]={3,0,0,12,2,0xf0,0x80,0x38,0,0,3,0xe9};
 assert(process_packet(join,12)==0&&s->joins==1);
 assert(process_packet(join,12)<0);join[11]=0xeb;assert(process_packet(join,12)==0&&s->phase==INFO);
 reset(INFO);
 unsigned char info[128]={0x40,0,0,0};w32(info+8,0x10);w16(info+14,14);w16(info+16,36);
 size_t at=24;for(unsigned i=0;i<7;i++)info[at+i*2]="desktop"[i];at+=16;
 for(unsigned i=0;i<18;i++){info[at+i*2]="wire-test-password"[i];}
 at+=38;at+=4;
 assert(client_info(info,at)==0&&s->authenticated&&s->phase==CONFIRM);
 reset(INFO);info[40]='X';assert(client_info(info,at)<0&&!s->authenticated&&s->cooldown>dm_clock_ms());
 reset(ACTIVE);s->authenticated=1;
 unsigned char input[16]={1,0,0,0};w16(input+8,0x8001);w16(input+10,0x9000);w16(input+12,65000);w16(input+14,65000);
 assert(!process_input(input,sizeof(input)));DmRemoteEvent e;assert(dm_rdp_event(&e)&&e.x==959&&e.y==543&&e.left);
 scan_event(0,0x1e);assert(dm_rdp_event(&e)&&!strcmp(e.text,"a"));scan_event(0,0x2a);scan_event(0,0x1e);assert(dm_rdp_event(&e)&&!strcmp(e.text,"A"));scan_event(0x8000,0x2a);
 scan_event(0,0x1d);scan_event(0,0x2e);assert(dm_rdp_event(&e)&&e.key==DM_KEY_COPY);scan_event(0x8000,0x1d);
 unicode_event(0xe9);assert(dm_rdp_event(&e)&&!strcmp(e.text,"\xc3\xa9"));
 for(unsigned iteration=0;iteration<10000;iteration++){
  size_t n=random_value()%sizeof(data);for(size_t i=0;i<n;i++)data[i]=random_value();
  reset(random_value()%8);if(n>=7&&iteration%2){data[0]=3;data[1]=0;b16(data+2,n);data[4]=2;data[5]=0xf0;data[6]=0x80;}
  process_packet(data,n);
  reset(ACTIVE);process_input(data,n);
  reset(INFO);client_info(data,n);
  reset(CONFIRM);confirm_active(data,n);
 }
 free(s);s=NULL;
 puts("PASS: RDP handshake truncation, channel joins, authentication rejection, clamped mouse, keyboard/UTF-8, 10000 malformed packets");
}
