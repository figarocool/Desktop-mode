/* Desktop Mode minimal TLS RDP server. Wire structures follow MS-RDPBCGR,
 * T.124/T.125. No WinPR/Windows dependency. See docs/remote-desktop.md. */
#include "rdp_server.h"
#include <openssl/ssl.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#ifdef DESKTOP_PREVIEW
#include <signal.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef DESKTOP_PREVIEW
#include <psp2/display.h>
#include <psp2/kernel/rng.h>
#else
extern int desktop_capture_rgba(void*,int);
#endif
#define WIDTH 960
#define HEIGHT 544
#define TILE_W 64
#define TILE_H 32
#define TILE_COUNT ((WIDTH/TILE_W)*(HEIGHT/TILE_H))
#define IO_CAP 65536
#define SHARE_ID 0x10000
#define USER_ID 1001
#define SERVER_ID 1002
#define GLOBAL_ID 1003
#define CERT_FILE "ux0:/data/desktop-mode/rdp-cert.pem"
#define KEY_FILE "ux0:/data/desktop-mode/rdp-key.pem"
enum {HELLO,TLS,MCS,CHANNELS,INFO,CONFIRM,FINAL,ACTIVE};
typedef struct {
 int listener,fd,phase,authenticated,left,right,shift,control,caps,joins,channel_count,attached;
 unsigned join_mask;
 unsigned port,protocols;
 unsigned short channels[16];
 SSL_CTX*ctx;
 SSL*ssl;
 unsigned char rx[IO_CAP],tx[IO_CAP];
 size_t rx_size,tx_size,tx_at;
 unsigned char*frame;
 uint64_t hashes[TILE_COUNT];
 int tile,frame_pending,force_frame;
 uint64_t deadline,next_capture,cooldown;
 unsigned failures;
 char password[128],status[160];
 DmRemoteEvent events[128];
 unsigned event_read,event_write;
} Server;
static Server*s;
static char last_status[160]="Server RDP disattivato";
static unsigned short le16(const unsigned char*p){return p[0]|((unsigned short)p[1]<<8);
 }
static unsigned be16(const unsigned char*p){return ((unsigned)p[0]<<8)|p[1];
 }
static unsigned long le32(const unsigned char*p){return (unsigned long)p[0]|((unsigned long)p[1]<<8)|((unsigned long)p[2]<<16)|((unsigned long)p[3]<<24);
 }
static void w16(unsigned char*p,unsigned v){p[0]=v;
 p[1]=v>>8;
 }
static void w32(unsigned char*p,unsigned long v){p[0]=v;
 p[1]=v>>8;
 p[2]=v>>16;
 p[3]=v>>24;
 }
static void b16(unsigned char*p,unsigned v){p[0]=v>>8;
 p[1]=v;
 }
static void status(const char*t){snprintf(last_status,sizeof(last_status),"%s",t);
 if(s)snprintf(s->status,sizeof(s->status),"%s",t);
#ifdef DESKTOP_PREVIEW
 if(getenv("DESKTOP_RDP_TRACE"))fprintf(stderr,"RDP status: %s\n",t);
#endif
}
static void trace(const char*t){
#ifdef DESKTOP_PREVIEW
 if(getenv("DESKTOP_RDP_TRACE"))fprintf(stderr,"RDP phase %d: %s\n",s?s->phase:-1,t);
#else
 (void)t;
#endif
}
static void disconnect(const char*reason){
 if(!s)return;
 trace(reason);
 if(s->ssl){SSL_free(s->ssl);
 s->ssl=NULL;
 }if(s->fd>=0){close(s->fd);
 s->fd=-1;
 }
 OPENSSL_cleanse(s->rx,sizeof(s->rx));
 OPENSSL_cleanse(s->tx,sizeof(s->tx));
 s->rx_size=s->tx_size=s->tx_at=0;
 s->phase=HELLO;
 s->authenticated=0;
 s->left=s->right=s->shift=s->control=s->caps=0;
 s->event_read=s->event_write=0;
 s->frame_pending=0;
 s->force_frame=1;
 status(reason);
}
static int nonblocking(int fd){int flags=fcntl(fd,F_GETFL,0);
 return flags<0?-1:fcntl(fd,F_SETFL,flags|O_NONBLOCK);
 }
static int queue_bytes(const void*data,size_t n){
 if(s->tx_at){memmove(s->tx,s->tx+s->tx_at,s->tx_size-s->tx_at);
 s->tx_size-=s->tx_at;
 s->tx_at=0;
 }
 if(n>IO_CAP-s->tx_size){disconnect("RDP: coda trasmissione piena");
 return -1;
 }
 memcpy(s->tx+s->tx_size,data,n);
 s->tx_size+=n;
 return 0;
}
static int x224(const unsigned char*body,size_t n){
 unsigned char header[7]={3,0,0,0,2,0xf0,0x80};
 if(n>65528)return -1;
 b16(header+2,n+7);
 return queue_bytes(header,7)<0?-1:queue_bytes(body,n);
}
static int send_mcs(const unsigned char*body,size_t n){
 unsigned char packet[16384];
 if(n>sizeof(packet)-8)return -1;
 packet[0]=0x68;
 b16(packet+1,SERVER_ID-1001);
 b16(packet+3,GLOBAL_ID);
 packet[5]=0x70;
 size_t offset=6;
 if(n>=128){packet[offset++]=0x80|(n>>8);
 packet[offset++]=n;
 }else packet[offset++]=n;
 memcpy(packet+offset,body,n);
 return x224(packet,n+offset);
}
static int send_share(unsigned type,const unsigned char*data,size_t n){
 unsigned char packet[16384];
 if(n>sizeof(packet)-6)return -1;
 w16(packet,n+6);
 w16(packet+2,0x10|type);
 w16(packet+4,SERVER_ID);
 memcpy(packet+6,data,n);
 return send_mcs(packet,n+6);
}
static int send_data(unsigned type,const unsigned char*data,size_t n){
 unsigned char packet[16384];
 if(n>sizeof(packet)-12)return -1;
 w32(packet,SHARE_ID);
 packet[4]=0;
 packet[5]=1;
 w16(packet+6,n+18);
 packet[8]=type;
 packet[9]=0;
 w16(packet+10,0);
 memcpy(packet+12,data,n);
 return send_share(7,packet,n+12);
}
static int ssl_context(void){
 #if OPENSSL_VERSION_NUMBER < 0x10100000L
 SSL_library_init();
 SSL_load_error_strings();
#ifndef DESKTOP_PREVIEW
 unsigned char seed[64];
 if(sceKernelGetRandomNumber(seed,sizeof(seed))<0)return -1;
 RAND_seed(seed,sizeof(seed));
 OPENSSL_cleanse(seed,sizeof(seed));
#endif
 s->ctx=SSL_CTX_new(SSLv23_server_method());
 if(!s->ctx)return -1;
 SSL_CTX_set_options(s->ctx,SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3|SSL_OP_NO_TLSv1|SSL_OP_NO_TLSv1_1);
#else
 s->ctx=SSL_CTX_new(TLS_server_method());
 if(!s->ctx)return -1;
 SSL_CTX_set_min_proto_version(s->ctx,TLS1_2_VERSION);
#endif
 SSL_CTX_set_options(s->ctx,SSL_OP_NO_COMPRESSION);
 char cert[8192],key[8192];
 X509*x=NULL;
 EVP_PKEY*k=NULL;
 BIO*b=NULL;
 if(dm_fs_read(CERT_FILE,cert,sizeof(cert))>0&&dm_fs_read(KEY_FILE,key,sizeof(key))>0){
  b=BIO_new_mem_buf(cert,-1);
 if(b){x=PEM_read_bio_X509(b,NULL,NULL,NULL);
 BIO_free(b);
 }
  b=BIO_new_mem_buf(key,-1);
 if(b){k=PEM_read_bio_PrivateKey(b,NULL,NULL,NULL);
 BIO_free(b);
 }
 }
 if(!x||!k||X509_check_private_key(x,k)!=1){
  X509_free(x);
 EVP_PKEY_free(k);
 x=NULL;
 k=NULL;
  EVP_PKEY_CTX*p=EVP_PKEY_CTX_new_id(EVP_PKEY_RSA,NULL);
  if(!p||EVP_PKEY_keygen_init(p)<=0||EVP_PKEY_CTX_set_rsa_keygen_bits(p,2048)<=0||EVP_PKEY_keygen(p,&k)<=0){EVP_PKEY_CTX_free(p);
 return -1;
 }EVP_PKEY_CTX_free(p);
  x=X509_new();
 if(!x){EVP_PKEY_free(k);
 return -1;
 }X509_set_version(x,2);
 ASN1_INTEGER_set(X509_get_serialNumber(x),1);
  X509_gmtime_adj(X509_get_notBefore(x),-3600);
 X509_gmtime_adj(X509_get_notAfter(x),315360000L);
 X509_set_pubkey(x,k);
  X509_NAME*name=X509_get_subject_name(x);
 X509_NAME_add_entry_by_txt(name,"CN",MBSTRING_ASC,(const unsigned char*)"Desktop Mode Vita",-1,-1,0);
 X509_set_issuer_name(x,name);
  if(!X509_sign(x,k,EVP_sha256())){X509_free(x);
 EVP_PKEY_free(k);
 return -1;
 }
  b=BIO_new(BIO_s_mem());
 if(b&&PEM_write_bio_X509(b,x)){char*bytes;
 long length=BIO_get_mem_data(b,&bytes);
 dm_fs_write(CERT_FILE,bytes,length,0);
 }BIO_free(b);
  b=BIO_new(BIO_s_mem());
 if(b&&PEM_write_bio_PrivateKey(b,k,NULL,NULL,0,NULL,NULL)){char*bytes;
 long length=BIO_get_mem_data(b,&bytes);
 dm_fs_write(KEY_FILE,bytes,length,0);
 }BIO_free(b);
 }
 int ok=SSL_CTX_use_certificate(s->ctx,x)==1&&SSL_CTX_use_PrivateKey(s->ctx,k)==1&&SSL_CTX_check_private_key(s->ctx)==1;
 X509_free(x);
 EVP_PKEY_free(k);
 OPENSSL_cleanse(key,sizeof(key));
 return ok?0:-1;
}
int dm_rdp_start(const char*password,unsigned port){
 if(!password||strlen(password)<8||strlen(password)>127||!port||port>65535){status("RDP: password 8-127 caratteri e porta 1-65535");
 return -1;
 }
 /* ASCII credentials avoid ambiguous Unicode normalization between clients. */
 for(const unsigned char*p=(const unsigned char*)password;*p;p++)if(*p<32||*p>126){status("RDP: usa una password ASCII");
 return -1;
 }
#ifdef DESKTOP_PREVIEW
 signal(SIGPIPE,SIG_IGN);
#endif
 dm_rdp_stop();
 DmSystemInfo info;
 dm_system_info(&info);
 s=calloc(1,sizeof(*s));
 if(!s){status("RDP: memoria insufficiente");
 return -1;
 }s->fd=s->listener=-1;
 s->port=port;
 s->force_frame=1;
 snprintf(s->password,sizeof(s->password),"%s",password);
 s->frame=malloc(WIDTH*HEIGHT*4);
 if(!s->frame||ssl_context()<0){dm_rdp_stop();
 status("RDP: inizializzazione TLS fallita");
 return -1;
 }
 s->listener=socket(AF_INET,SOCK_STREAM,0);
 int reuse=1;
 if(s->listener>=0)setsockopt(s->listener,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
 struct sockaddr_in address;
 memset(&address,0,sizeof(address));
 address.sin_family=AF_INET;
 address.sin_port=htons(port);
 address.sin_addr.s_addr=htonl(INADDR_ANY);
 if(s->listener<0||nonblocking(s->listener)<0||bind(s->listener,(struct sockaddr*)&address,sizeof(address))<0||listen(s->listener,1)<0){dm_rdp_stop();
 status("RDP: porta non disponibile");
 return -1;
 }
 status("RDP in ascolto; utente: desktop");
 return 0;
}
void dm_rdp_stop(void){if(!s)return;
 disconnect("Server RDP disattivato");
 if(s->listener>=0)close(s->listener);
 SSL_CTX_free(s->ctx);
 free(s->frame);
 OPENSSL_cleanse(s->password,sizeof(s->password));
 free(s);
 s=NULL;
 }
int dm_rdp_enabled(void){return s!=NULL;
 }
int dm_rdp_connected(void){return s&&s->phase==ACTIVE&&s->authenticated;
 }
unsigned dm_rdp_port(void){return s?s->port:3389;
 }
const char*dm_rdp_status(void){return last_status;
 }
static void push_event(DmRemoteEvent event){if(s->event_write-s->event_read>=128){disconnect("RDP: troppi eventi input");
 return;
 }s->events[s->event_write++%128]=event;
 }
int dm_rdp_event(DmRemoteEvent*event){if(!s||s->event_read==s->event_write)return 0;
 *event=s->events[s->event_read++%128];
 return 1;
 }
static void unicode_event(unsigned code){
 if(code>=0xd800&&code<=0xdfff)return;
 DmRemoteEvent e={0};
 e.kind=2;
 if(code<128){if(code<32)return;
 e.text[0]=code;
 }
 else if(code<2048){e.text[0]=0xc0|(code>>6);
 e.text[1]=0x80|(code&63);
 }
 else{e.text[0]=0xe0|(code>>12);
 e.text[1]=0x80|((code>>6)&63);
 e.text[2]=0x80|(code&63);
 }push_event(e);
}
static void scan_event(unsigned flags,unsigned code){
 int release=flags&0x8000,extended=flags&0x100;
 if(code==0x2a||code==0x36){s->shift=!release;
 return;
 }
 if(code==0x1d){s->control=!release;
 return;
 }
 if(release)return;
 if(code==0x3a){s->caps=!s->caps;
 return;
 }
 DmRemoteEvent e={0};
 e.kind=2;
 if(code==0x1c)e.key=DM_KEY_ENTER;
 else if(code==0x0e)e.key=DM_KEY_BACKSPACE;
 else if(extended&&code==0x53)e.key=DM_KEY_DELETE;
 else if(extended&&code==0x4b)e.key=DM_KEY_LEFT;
 else if(extended&&code==0x4d)e.key=DM_KEY_RIGHT;
 else if(extended&&code==0x48)e.key=DM_KEY_UP;
 else if(extended&&code==0x50)e.key=DM_KEY_DOWN;
 else if(extended&&code==0x47)e.key=DM_KEY_HOME;
 else if(extended&&code==0x4f)e.key=DM_KEY_END;
 if(e.key){if(s->shift)e.key|=DM_KEY_SHIFT;
 push_event(e);
 return;
 }
 const char*plain="\0\0" "1234567890-=\0\0qwertyuiop[]\0\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
 const char*shift="\0\0" "!@#$%^&*()_+\0\0QWERTYUIOP{}\0\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*\0 ";
 /* Embedded NULs are intentional: indexed by set-1 scancode. */
 if(code>=0x3a)return;
 char c=plain[code];
 if(s->control){if(c=='a')e.key=DM_KEY_SELECT_ALL;
 if(c=='c')e.key=DM_KEY_COPY;
 if(c=='v')e.key=DM_KEY_PASTE;
 if(c=='x')e.key=DM_KEY_CUT;
 if(c=='s')e.key=DM_KEY_SAVE;
 if(e.key)push_event(e);
 return;
 }
 if(s->shift)c=shift[code];
 else if(s->caps&&c>='a'&&c<='z')c-=32;
 if(c){e.text[0]=c;
 push_event(e);
 }
}
static int process_input(const unsigned char*p,size_t n){
 if(n<4)return -1;
 unsigned count=le16(p);
 if(count>128||n!=4+count*12)return -1;
 for(unsigned i=0;i<count;i++){const unsigned char*v=p+4+i*12;
 unsigned type=le16(v+4),flags=le16(v+6),a=le16(v+8),b=le16(v+10);
  if(type==4)scan_event(flags,a);
 else if(type==5){if(!(flags&0x8000)){if(a==13){DmRemoteEvent e={0};
 e.kind=2;
 e.key=DM_KEY_ENTER;
 push_event(e);
 }else unicode_event(a);
 }}
  else if(type==0x8001){DmRemoteEvent e={0};
 e.kind=1;
 e.x=a>=WIDTH?WIDTH-1:a;
 e.y=b>=HEIGHT?HEIGHT-1:b;
   if(flags&0x1000)s->left=!!(flags&0x8000);
 if(flags&0x2000)s->right=!!(flags&0x8000);
 e.modifiers=(s->control?1:0)|(s->shift?2:0);
 e.left=s->left;
 e.right=s->right;
   if(flags&0x200)e.key=(flags&0x100)?DM_KEY_SCROLL_DOWN:DM_KEY_SCROLL_UP;
 push_event(e);
  }else if(type!=0)return -1;
  if(s->fd<0)return -1;
 }return 0;
}
static size_t ber_length(unsigned char*p,size_t n){if(n<128){p[0]=n;
 return 1;
 }if(n<256){p[0]=0x81;
 p[1]=n;
 return 2;
 }p[0]=0x82;
 b16(p+1,n);
 return 3;
 }
static size_t per_length(unsigned char*p,size_t n){if(n<128){p[0]=n;
 return 1;
 }p[0]=0x80|(n>>8);
 p[1]=n;
 return 2;
 }
static int connect_response(const unsigned char*p,size_t n){
 if(n<3||p[0]!=0x7f||p[1]!=0x65)return -1;
 /* Locate the first GCC Core block, then walk only contiguous length-checked blocks. */
 size_t at=0;
 for(size_t i=2;i+8<=n;i++)if(le16(p+i)==0xc001&&le16(p+i+2)>=128&&le16(p+i+2)<=n-i&&(le32(p+i+4)&0xffff0000UL)==0x80000UL){at=i;
 break;
 }
 if(!at)return -1;
 s->channel_count=0;
 while(at+4<=n){unsigned type=le16(p+at),length=le16(p+at+2);
 if(length<4||length>n-at)return -1;
  if(type==0xc003){if(length<8)return -1;
 unsigned count=le32(p+at+4);
 if(count>16||length!=8+count*12)return -1;
 s->channel_count=count;
 for(unsigned i=0;i<count;i++)s->channels[i]=1004+i;
 }
  at+=length;
 }
 if(at!=n)return -1;
 unsigned char blocks[128]={0};
 size_t z=0;
 w16(blocks+z,0x0c01);
 w16(blocks+z+2,12);
 w32(blocks+z+4,0x80004);
 w32(blocks+z+8,s->protocols);
 z+=12;
 w16(blocks+z,0x0c02);
 w16(blocks+z+2,12);
 z+=12;
 /* TLS: no RDP RC4 layer. */
 unsigned network_length=8+s->channel_count*2+(s->channel_count%2?2:0);
 w16(blocks+z,0x0c03);
 w16(blocks+z+2,network_length);
 w16(blocks+z+4,GLOBAL_ID);
 w16(blocks+z+6,s->channel_count);
 for(int i=0;i<s->channel_count;i++)w16(blocks+z+8+i*2,s->channels[i]);
 z+=network_length;
 unsigned char gcc[256]={0,5,0,0x14,0x7c,0,1,0x2a,0x14,0,1,1,1,0,1,0xc0,0,'M','c','D','n'};
 size_t g=21;
 g+=per_length(gcc+g,z);
 memcpy(gcc+g,blocks,z);
 g+=z;
 unsigned char body[512]={0x0a,1,0,2,1,0,0x30,0x1a,2,1,0x22,2,1,2,2,1,0,2,1,1,2,1,0,2,1,1,2,3,0,0xff,0xff,2,1,2};
 size_t len=34;
 body[len++]=4;
 len+=ber_length(body+len,g);
 memcpy(body+len,gcc,g);
 len+=g;
 unsigned char response[768]={0x7f,0x66};
 size_t h=2;
 h+=ber_length(response+h,len);
 memcpy(response+h,body,len);
#ifdef DM_RDP_TEST
 if(getenv("DESKTOP_RDP_TRACE")){fprintf(stderr,"MCS RESPONSE ");
 for(size_t i=0;i<h+len;i++)fprintf(stderr,"%02x",response[i]);
 fprintf(stderr,"\n");
 }
#endif
 return x224(response,h+len);
}
static size_t capability(unsigned char*p,unsigned type,unsigned length){memset(p,0,length);
 w16(p,type);
 w16(p+2,length);
 return length;
 }
static int demand_active(void){
 unsigned char caps[512];
 size_t n=0;
 unsigned count=0;
 unsigned char*p;
 p=caps+n;
 n+=capability(p,1,24);
 count++;
 w16(p+4,4);
 w16(p+6,7);
 w16(p+8,0x200);
 /* no fast path, compression or autoreconnect */
 p=caps+n;
 n+=capability(p,2,28);
 count++;
 w16(p+4,16);
 w16(p+6,1);
 w16(p+8,1);
 w16(p+10,1);
 w16(p+12,WIDTH);
 w16(p+14,HEIGHT);
 w16(p+18,1);
 w16(p+20,1);
 w16(p+24,1);
 p=caps+n;
 n+=capability(p,3,88);
 count++;
 w16(p+24,1);
 w16(p+30,2);
 /* orderSupport array all zero: bitmap updates only */
 p=caps+n;
 n+=capability(p,8,8);
 count++;
 w16(p+4,1);
 w16(p+6,1);
 /* system pointer is hidden; shell draws its own */
 p=caps+n;
 n+=capability(p,13,88);
 count++;
 w16(p+4,0x11);
 w32(p+8,0x409);
 w32(p+12,4);
 w32(p+20,12);
 p=caps+n;
 n+=capability(p,9,8);
 count++;
 w16(p+4,SERVER_ID);
 p=caps+n;
 n+=capability(p,14,8);
 count++;
 w16(p+4,1);
 unsigned char body[768];
 w32(body,SHARE_ID);
 w16(body+4,4);
 w16(body+6,n+4);
 memcpy(body+8,"DM\0\0",4);
 w16(body+12,count);
 w16(body+14,0);
 memcpy(body+16,caps,n);
 w32(body+16+n,0);
 return send_share(1,body,20+n);
}
static int client_info(const unsigned char*p,size_t n){
 if(n<22||le16(p)!=0x40)return -1;
 p+=4;
 n-=4;
 unsigned flags=le32(p+4),lengths[5];
 if(!(flags&0x10))return -1;
 for(int i=0;i<5;i++){lengths[i]=le16(p+8+i*2);
 if(lengths[i]%2)return -1;
 }
 size_t at=18;
 const unsigned char*user=NULL,*pass=NULL;
 for(int i=0;i<5;i++){if(lengths[i]+2>n-at)return -1;
 if(p[at+lengths[i]]||p[at+lengths[i]+1])return -1;
 if(i==1)user=p+at;
 if(i==2)pass=p+at;
 at+=lengths[i]+2;
 }
 const char*name="desktop";
 size_t name_len=strlen(name),pass_len=strlen(s->password);
 unsigned mismatch=lengths[1]!=name_len*2||lengths[2]!=pass_len*2;
 if(!mismatch){for(size_t i=0;i<name_len;i++)mismatch|=user[i*2]^(unsigned char)name[i],mismatch|=user[i*2+1];
 for(size_t i=0;i<pass_len;i++)mismatch|=pass[i*2]^(unsigned char)s->password[i],mismatch|=pass[i*2+1];
 }
 if(mismatch){s->failures++;
 s->cooldown=dm_clock_ms()+(s->failures>=5?30000:2000);
 return -1;
 }
 s->authenticated=1;
 s->failures=0;
 unsigned char license[20]={0x80,0,0,0,0xff,3,16,0,7,0,0,0,2,0,0,0,4,0,0,0};
 if(send_mcs(license,sizeof(license))<0||demand_active()<0)return -1;
 s->phase=CONFIRM;
 trace("password accepted; Demand Active");
 return 0;
}
static int confirm_active(const unsigned char*p,size_t n){
 if(n<16||le32(p+6)!=SHARE_ID)return -1;
 unsigned source=le16(p+12),combined=le16(p+14);
 size_t at=16+source;
 if(at+4>n||combined<4||combined>n-at)return -1;
 unsigned count=le16(p+at);
 at+=4;
 if(count>64)return -1;
 int bitmap=0;
 for(unsigned i=0;i<count;i++){if(at+4>n)return -1;
 unsigned type=le16(p+at),len=le16(p+at+2);
 if(len<4||len>n-at)return -1;
  if(type==2){if(len<28||le16(p+at+4)!=16)return -1;
 bitmap=1;
 }at+=len;
 }
 if(!bitmap)return -1;
 s->phase=FINAL;
 unsigned char sync[4]={1,0,0xe9,3},cooperate[8]={4,0,0,0,0,0,0,0},grant[8]={2,0,0xe9,3,0xea,3,0,0};
 if(send_data(31,sync,4)<0||send_data(20,cooperate,8)<0||send_data(20,grant,8)<0)return -1;
 trace("Confirm Active accepted");
 return 0;
}
static int process_packet(const unsigned char*p,size_t n){
#ifdef DM_RDP_TEST
 if(getenv("DESKTOP_RDP_TRACE")){fprintf(stderr,"PACKET phase=%d n=%zu ",s->phase,n);
 for(size_t i=0;i<n&&i<35;i++)fprintf(stderr,"%02x",p[i]);
 fprintf(stderr,"\n");
 }
#endif

 if(s->phase==HELLO){
  if(n<19||p[0]!=3||p[5]!=0xe0||n>260||p[4]!=n-5)return -1;
 const unsigned char*neg=NULL;
 for(size_t i=11;i+8<=n;i++)if(p[i]==1&&le16(p+i+2)==8){
     if(i+8==n||(i+44==n&&p[i+8]==6&&le16(p+i+10)==36)){neg=p+i;break;}
 }
 if(!neg)return -1;
  if(neg[0]!=1||le16(neg+2)!=8)return -1;
 s->protocols=le32(neg+4);
 if(!(s->protocols&1))return -1;
  unsigned char response[19]={3,0,0,19,14,0xd0,0,0,0,0,0,2,0,8,0,1,0,0,0};
  if(queue_bytes(response,19)<0)return -1;
 s->phase=TLS;
 trace("X224 TLS selected");
 return 0;
 }
 if(n<8||p[0]!=3||p[4]!=2||p[5]!=0xf0||p[6]!=0x80)return -1;
 p+=7;
 n-=7;
 if(s->phase==MCS){if(connect_response(p,n)<0)return -1;
 s->phase=CHANNELS;
 s->joins=0;s->join_mask=0;s->attached=0;
 trace("MCS Connect Response");
 return 0;
 }
 if(s->phase==CHANNELS){
  if(p[0]==4)return n==5?0:-1;
 /* Erect Domain Request */
  if(p[0]==0x28){unsigned char attach[4]={0x2e,0,0,0};
 if(n!=1||s->attached)return -1;
 s->attached=1;
 return x224(attach,4);
 }
  if(p[0]==0x38){if(n!=5||be16(p+1)!=0||!s->attached)return -1;
 unsigned channel=be16(p+3);
 int index=channel==USER_ID?0:channel==GLOBAL_ID?1:-1;
 for(int i=0;i<s->channel_count;i++)if(channel==s->channels[i])index=i+2;
 if(index<0||(s->join_mask&(1u<<index)))return -1;
 s->join_mask|=1u<<index;
   unsigned char join[8]={0x3e,0,0,0,0,0,0,0};
 b16(join+4,channel);
 b16(join+6,channel);
 s->joins++;
 if(s->joins==s->channel_count+2)s->phase=INFO;
 return x224(join,8);
  }return -1;
 }
 if(p[0]==0x20)return -1;
 /* Disconnect Provider Ultimatum */
 if(n<7||p[0]!=0x64||be16(p+1)!=0)return -1;
 unsigned channel=be16(p+3);
 size_t offset=7,length=p[6];
 if(length&0x80){if(n<8)return -1;
 length=((length&0x7f)<<8)|p[7];
 offset=8;
 }if(length!=n-offset)return -1;
 p+=offset;
 n=length;
 if(channel!=GLOBAL_ID)return 0;
 /* negotiated virtual channels have no service */
 if(s->phase==INFO)return client_info(p,n);
 if(n<6||le16(p)!=n)return -1;
 unsigned type=le16(p+2)&15;
 if(s->phase==CONFIRM)return type==3?confirm_active(p,n):-1;
 if(type!=7||n<18||le32(p+6)!=SHARE_ID||p[15])return -1;
 unsigned data_type=p[14];
 if(data_type==39&&s->phase==FINAL){unsigned char fontmap[8]={0,0,0,0,3,0,4,0};
 if(send_data(40,fontmap,8)<0)return -1;
 s->phase=ACTIVE;
 s->deadline=0;
 s->force_frame=1;
 status("RDP connesso: desktop condiviso");
 trace("ACTIVE");
 unsigned char pointer[8]={1,0,0,0,0,0,0,0};
 return send_data(27,pointer,8);
 }
 if(data_type==28&&s->phase==ACTIVE)return process_input(p+18,n-18);
 if(data_type==33&&s->phase==ACTIVE){s->force_frame=1;
 return 0;
 }
 if(data_type==35&&s->phase==ACTIVE){if(n!=22)return -1;
 return 0;
 }
 if(data_type==31||data_type==20||data_type==39||data_type==43)return 0;
 return 0;
}
static int flush(void){
 if(s->tx_at==s->tx_size){s->tx_at=s->tx_size=0;
 return 0;
 }
 int sent;
 if(s->ssl){sent=SSL_write(s->ssl,s->tx+s->tx_at,s->tx_size-s->tx_at);
 if(sent<=0){int e=SSL_get_error(s->ssl,sent);
 if(e==SSL_ERROR_WANT_READ||e==SSL_ERROR_WANT_WRITE)return 0;
 return -1;
 }}
 else{sent=send(s->fd,s->tx+s->tx_at,s->tx_size-s->tx_at,0);
 if(sent<0&&(errno==EAGAIN||errno==EWOULDBLOCK))return 0;
 if(sent<=0)return -1;
 }
 s->tx_at+=sent;
 if(s->tx_at==s->tx_size)s->tx_at=s->tx_size=0;
 return 0;
}
static int tile_packet(int tile){
 unsigned x=(tile%(WIDTH/TILE_W))*TILE_W,y=(tile/(WIDTH/TILE_W))*TILE_H;
 uint64_t hash=1469598103934665603ULL;
 for(unsigned row=0;row<TILE_H;row++){const unsigned char*p=s->frame+((y+row)*WIDTH+x)*4;
 for(unsigned i=0;i<TILE_W*4;i++){hash^=p[i];
 hash*=1099511628211ULL;
 }}
 if(!s->force_frame&&hash==s->hashes[tile])return 0;
 s->hashes[tile]=hash;
 unsigned char packet[4+18+TILE_W*TILE_H*2];
 w16(packet,1);
 w16(packet+2,1);
 w16(packet+4,x);
 w16(packet+6,y);
 w16(packet+8,x+TILE_W-1);
 w16(packet+10,y+TILE_H-1);
 w16(packet+12,TILE_W);
 w16(packet+14,TILE_H);
 w16(packet+16,16);
 w16(packet+18,0);
 w16(packet+20,TILE_W*TILE_H*2);
 for(unsigned row=0;row<TILE_H;row++){const unsigned char*p=s->frame+((y+TILE_H-1-row)*WIDTH+x)*4;
 unsigned char*out=packet+22+row*TILE_W*2;
 for(unsigned col=0;col<TILE_W;col++,p+=4){unsigned rgb=((p[0]>>3)<<11)|((p[1]>>2)<<5)|(p[2]>>3);
 w16(out+col*2,rgb);
 }}
 return send_data(2,packet,sizeof(packet));
}
void dm_rdp_capture(void){
 if(!dm_rdp_connected()||s->frame_pending||dm_clock_ms()<s->next_capture)return;
#ifdef DESKTOP_PREVIEW
 if(desktop_capture_rgba(s->frame,WIDTH*4)<0)return;
#else
 SceDisplayFrameBuf frame;
 memset(&frame,0,sizeof(frame));
 frame.size=sizeof(frame);
 if(sceDisplayGetFrameBuf(&frame,SCE_DISPLAY_SETBUF_IMMEDIATE)<0||!frame.base||frame.width<WIDTH||frame.height<HEIGHT||frame.pixelformat!=SCE_DISPLAY_PIXELFORMAT_A8B8G8R8)return;
 for(unsigned y=0;y<HEIGHT;y++)memcpy(s->frame+y*WIDTH*4,(unsigned char*)frame.base+y*frame.pitch*4,WIDTH*4);
#endif
 s->tile=0;
 s->frame_pending=1;
 s->next_capture=dm_clock_ms()+250;
}
void dm_rdp_tick(void){
 if(!s)return;
 uint64_t now=dm_clock_ms();
 if(s->fd<0){struct sockaddr_in peer;
 socklen_t length=sizeof(peer);
 int fd=accept(s->listener,(struct sockaddr*)&peer,&length);
 if(fd<0)return;
 if(now<s->cooldown||nonblocking(fd)<0){close(fd);
 return;
 }s->fd=fd;
 s->phase=HELLO;
 s->deadline=now+30000;
 status("RDP: connessione in corso");
 trace("client accepted");
 }
 if(s->deadline&&now>s->deadline){disconnect("RDP: timeout connessione");
 return;
 }
 if(flush()<0){disconnect("RDP: connessione terminata");
 return;
 }
 if(s->tx_size)return;
 if(s->phase==TLS){if(s->tx_size)return;
 if(!s->ssl){s->ssl=SSL_new(s->ctx);
 if(!s->ssl||SSL_set_fd(s->ssl,s->fd)!=1){disconnect("RDP: TLS non disponibile");
 return;
 }SSL_set_accept_state(s->ssl);
 }int result=SSL_accept(s->ssl);
 if(result!=1){int e=SSL_get_error(s->ssl,result);
 if(e==SSL_ERROR_WANT_READ||e==SSL_ERROR_WANT_WRITE)return;
 disconnect("RDP: handshake TLS fallito");
 return;
 }s->phase=MCS;
 trace("TLS established");
 }
 for(unsigned iteration=0;iteration<8;iteration++){
  if(s->rx_size>=4){if(s->rx[0]!=3||s->rx[1]){disconnect("RDP: intestazione non valida");
 return;
 }unsigned length=be16(s->rx+2);
 if(length<7||length>32768){disconnect("RDP: pacchetto fuori limite");
 return;
 }
   if(s->rx_size>=length){int result=process_packet(s->rx,length);
 if(result<0||s->fd<0){disconnect("RDP: credenziali o pacchetto non valido");
 return;
 }memmove(s->rx,s->rx+length,s->rx_size-length);
 s->rx_size-=length;
 if(s->phase==TLS)break;
 continue;
 }}
  if(s->rx_size==IO_CAP){disconnect("RDP: ricezione fuori limite");
 return;
 }
  int got;
 if(s->ssl){got=SSL_read(s->ssl,s->rx+s->rx_size,IO_CAP-s->rx_size);
 if(got<=0){int e=SSL_get_error(s->ssl,got);
 if(e==SSL_ERROR_WANT_READ||e==SSL_ERROR_WANT_WRITE)break;
 disconnect("RDP: client disconnesso");
 return;
 }}
  else{got=recv(s->fd,s->rx+s->rx_size,IO_CAP-s->rx_size,0);
 if(got<0&&(errno==EAGAIN||errno==EWOULDBLOCK))break;
 if(got<=0){disconnect("RDP: client disconnesso");
 return;
 }}
  s->rx_size+=got;
 }
 if(s->frame_pending&&s->phase==ACTIVE){unsigned sent=0;
 while(s->tile<TILE_COUNT&&sent<8&&s->tx_size-s->tx_at<32768){if(tile_packet(s->tile++)<0)return;
 sent++;
 }if(s->tile==TILE_COUNT){s->frame_pending=0;
 s->force_frame=0;
 }}
 if(s->fd>=0&&flush()<0)disconnect("RDP: connessione terminata");
}
#ifdef DM_RDP_TEST
/* Test transport uses memory BIOs, never opens a network socket. */
int dm_rdp_test_begin(const char*password){
 dm_rdp_stop();
 s=calloc(1,sizeof(*s));
 if(!s)return -1;
 s->fd=123456;
 s->listener=-1;
 s->force_frame=1;
 snprintf(s->password,sizeof(s->password),"%s",password);
 s->frame=calloc(WIDTH*HEIGHT,4);
 if(!s->frame||ssl_context()<0)return -1;
 SSL_CTX_set_max_proto_version(s->ctx,TLS1_2_VERSION); /* Match VitaSDK TLS 1.2 in this test. */
 return 0;
}
static int test_pump(void){
 if(s->phase==TLS&&!s->ssl&&!s->tx_size){s->ssl=SSL_new(s->ctx);
 if(!s->ssl)return -1;
 BIO*in=BIO_new(BIO_s_mem()),*out=BIO_new(BIO_s_mem());
 if(!in||!out){BIO_free(in);
 BIO_free(out);
 return -1;
 }BIO_set_mem_eof_return(in,-1);
 BIO_set_mem_eof_return(out,-1);
 SSL_set_bio(s->ssl,in,out);
 SSL_set_accept_state(s->ssl);
 }
 if(s->phase==TLS&&s->ssl){int result=SSL_accept(s->ssl);
 if(result==1)s->phase=MCS;
 else{int e=SSL_get_error(s->ssl,result);
 if(e!=SSL_ERROR_WANT_READ&&e!=SSL_ERROR_WANT_WRITE)return -1;
 }}
 if(s->phase>=MCS){for(unsigned i=0;i<32;i++){
   if(s->rx_size>=4){unsigned n=be16(s->rx+2);
 if(s->rx[0]!=3||s->rx[1]||n<7||n>32768)return -1;
 if(n<=s->rx_size){if(process_packet(s->rx,n)<0)return -1;
 memmove(s->rx,s->rx+n,s->rx_size-n);
 s->rx_size-=n;
 continue;
 }}
   int got=SSL_read(s->ssl,s->rx+s->rx_size,IO_CAP-s->rx_size);
 if(got<=0){int e=SSL_get_error(s->ssl,got);
 if(e!=SSL_ERROR_WANT_READ&&e!=SSL_ERROR_WANT_WRITE)return -1;
 break;
 }s->rx_size+=got;
  }
  if(s->phase==ACTIVE&&s->tx_size<30000&&s->tile<TILE_COUNT){for(unsigned i=0;i<6&&s->tile<TILE_COUNT;i++)if(tile_packet(s->tile++)<0)return -1;
 }
  if(s->tx_size){int result=SSL_write(s->ssl,s->tx,s->tx_size);
 if(result!=(int)s->tx_size)return -1;
 s->tx_size=s->tx_at=0;
 }
 }
 return 0;
}
int dm_rdp_test_feed(const unsigned char*bytes,int n){
 if(!s||s->fd<0||n<0)return -1;
 if(s->phase==HELLO){if((size_t)n>IO_CAP-s->rx_size)return -1;
 memcpy(s->rx+s->rx_size,bytes,n);
 s->rx_size+=n;
 if(s->rx_size>=4){unsigned length=be16(s->rx+2);
 if(length<7||length>32768)return -1;
 if(s->rx_size>=length){if(process_packet(s->rx,length)<0)return -1;
 memmove(s->rx,s->rx+length,s->rx_size-length);
 s->rx_size-=length;
 }}return 0;
 }
 if(test_pump()<0)return -1;
 if(n&&(!s->ssl||BIO_write(SSL_get_rbio(s->ssl),bytes,n)!=n))return -1;
 return test_pump();
}
int dm_rdp_test_read(unsigned char*bytes,int n){
 if(!s||s->fd<0||n<=0)return -1;
 if(s->phase==TLS&&!s->ssl&&s->tx_size){size_t count=s->tx_size<(size_t)n?s->tx_size:(size_t)n;
 memcpy(bytes,s->tx,count);
 memmove(s->tx,s->tx+count,s->tx_size-count);
 s->tx_size-=count;
 return count;
 }
 if(test_pump()<0)return -1;
 int got= s->ssl?BIO_read(SSL_get_wbio(s->ssl),bytes,n):0;
 return got>0?got:0;
}
int dm_rdp_test_phase(void){return s?s->phase:-1;
 }
int dm_rdp_test_fill(void){if(!s)return -1;
 for(unsigned y=0;y<HEIGHT;y++)for(unsigned x=0;x<WIDTH;x++){unsigned char*p=s->frame+(y*WIDTH+x)*4;
 p[0]=x<WIDTH/2?255:0;
 p[1]=y<HEIGHT/2?0:255;
 p[2]=x>=WIDTH/2?255:0;
 p[3]=255;
 }return 0;
 }
#endif
