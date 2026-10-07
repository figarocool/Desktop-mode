#include "rdp_server.h"
#include "preferences.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {unsigned port;
 char password[128];
 } RemotePanel;
static void open_remote(DmWindow*w,const char*arg){(void)arg;
 ((RemotePanel*)w->state)->port=dm_rdp_port();
 }
static void password_result(const char*text,void*ctx){DmWindow*w=ctx;
 if(!text||!w->used)return;
 if(dm_rdp_enabled()){dm_status("Disabilita il server prima di cambiare password");return;}
 RemotePanel*p=w->state;
 size_t n=strlen(text);
 if(n<8||n>127){dm_status("Password RDP: da 8 a 127 caratteri ASCII");
 return;
 }for(size_t i=0;i<n;i++)if((unsigned char)text[i]<32||(unsigned char)text[i]>126){dm_status("Password RDP: usa caratteri ASCII");
 return;
 }snprintf(p->password,sizeof(p->password),"%s",text);
 }
static void port_result(const char*text,void*ctx){DmWindow*w=ctx;
 if(!text||!w->used)return;
 if(dm_rdp_enabled()){dm_status("Disabilita il server prima di cambiare porta");
 return;
 }char*end;
 long port=strtol(text,&end,10);
 if(!*text||*end||port<1||port>65535){dm_status("Porta RDP non valida");
 return;
 }((RemotePanel*)w->state)->port=port;
 }
static void draw_remote(DmWindow*w){RemotePanel*p=w->state;
 DmSystemInfo info;
 dm_system_info(&info);if(info.preview)snprintf(info.ip,sizeof(info.ip),"127.0.0.1 (PC locale)");
 unsigned ink=DM_COLOR(24,38,55,255);
 char text[160];
 dm_text(w->x+25,w->y+70,dm_localize("Condivide questo desktop con un client RDP.","Shares this desktop with an RDP client.","Comparte este escritorio con un cliente RDP."),ink);
 snprintf(text,sizeof(text),"IP: %s    Porta: %u",info.ip,dm_rdp_enabled()?dm_rdp_port():p->port);
 dm_text(w->x+25,w->y+110,text,ink);
 dm_text(w->x+25,w->y+150,"Utente: desktop | TLS | 960 x 544",ink);
 dm_text(w->x+25,w->y+190,dm_localize(dm_rdp_enabled()?"Password attiva nel server":p->password[0]?"Password impostata per questa sessione":"Imposta una password prima di abilitare il server",dm_rdp_enabled()?"Password active on server":p->password[0]?"Password set for this session":"Set a password before enabling the server",dm_rdp_enabled()?"Contrasena activa en el servidor":p->password[0]?"Contrasena configurada para esta sesion":"Configura una contrasena antes de activar el servidor"),ink);
 const char*buttons[]={dm_localize("Password...","Password...","Contrasena..."),dm_localize("Porta...","Port...","Puerto..."),dm_localize(dm_rdp_enabled()?"Disabilita server":"Abilita server",dm_rdp_enabled()?"Disable server":"Enable server",dm_rdp_enabled()?"Desactivar servidor":"Activar servidor")};
 for(int i=0;i<3;i++){dm_rect(w->x+25+i*205,w->y+215,195,35,DM_COLOR(198,219,240,255));
 dm_text(w->x+35+i*205,w->y+239,buttons[i],ink);
 }
 dm_text(w->x+25,w->y+290,dm_rdp_status(),ink);
 dm_text(w->x+25,w->y+335,dm_localize("Client: TLS senza NLA; un collegamento alla volta.","Client: TLS without NLA; one connection at a time.","Cliente: TLS sin NLA; una conexion a la vez."),ink);
 dm_text(w->x+25,w->y+370,dm_localize("Mouse e tastiera; audio e clipboard remota non inclusi.","Mouse and keyboard; audio and remote clipboard are not included.","Raton y teclado; audio y portapapeles remoto no incluidos."),ink);
}
static void click_remote(DmWindow*w,int x,int y){RemotePanel*p=w->state;
 if(y<215||y>=250)return;
 if(x>=25&&x<220)dm_prompt("Password RDP (8-127 caratteri ASCII)","",password_result,w);
 else if(x>=230&&x<425){char text[16];
 snprintf(text,sizeof(text),"%u",p->port);
 dm_prompt("Porta RDP",text,port_result,w);
 }else if(x>=435&&x<630){if(dm_rdp_enabled())dm_rdp_stop();
 else dm_rdp_start(p->password,p->port);
 }}
static int close_remote(DmWindow*w){RemotePanel*p=w->state;
 volatile char*bytes=p->password;
 for(unsigned i=0;i<sizeof(p->password);i++)bytes[i]=0;
 return 1;
 }
const DmApp dm_remote_app={DM_API_VERSION,"remote","Desktop remoto RDP",sizeof(RemotePanel),open_remote,draw_remote,click_remote,0,0,close_remote,0,0};
