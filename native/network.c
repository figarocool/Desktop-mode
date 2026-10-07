#include "desktop_api.h"
#include "network_service.h"
#include <stdint.h>
#include <time.h>
#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include <smb2/libsmb2-raw.h>
#include <smb2/libsmb2-share-enum.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef DESKTOP_PREVIEW
#include <ifaddrs.h>
#else
#include <psp2/net/netctl.h>
#endif
extern void dm_files_changed(void);
#define INK DM_COLOR(24,38,55,255)
#define MAX_ROWS 512
static const int probe_ports[]= {
    445,139,80,443,22
};
typedef struct {
    int fd,port;
    uint32_t ip;
    uint64_t deadline;
}
Probe;
typedef struct {
    char name[256];
    int directory,smb;
    uint64_t size;
}
NetRow;
typedef struct {
    struct smb2_context*smb;
    struct smb2fh*file;
    NetRow rows[MAX_ROWS];
    int count,offset,selected,last_row,click_valid,mode,busy,scanning;
    char server[64],share[256],path[1024],user[128],password[128],message[256],remote_file[1024],local_file[1024],partial_file[1024],rename_source[1024],rename_target[1024];
    char upload_local[1024],upload_temp[1024],upload_target[1024];
    uint32_t first,last,next,local;
    int next_port,tested;
    Probe probes[24];
    uint64_t clicked,downloaded,download_size,uploaded,upload_size;
    int local_fd,partial,download_active,connected,replace,upload_fd,upload_active,upload_chunk,upload_chunk_written,upload_temp_created;
    unsigned char buffer[65536];
}
Network;
static int local_subnet(uint32_t*ip,uint32_t*mask) {
#ifdef DESKTOP_PREVIEW
    struct ifaddrs*all=NULL;
    if(getifaddrs(&all)<0)return -1;
    int found=0;
    for(struct ifaddrs*i=all;i;i=i->ifa_next) {
        if(!i->ifa_addr||!i->ifa_netmask||i->ifa_addr->sa_family!=AF_INET)continue;
        uint32_t address=ntohl(((struct sockaddr_in*)i->ifa_addr)->sin_addr.s_addr);
        if((address>>24)==127||address==0)continue;
        *ip=address;
        *mask=ntohl(((struct sockaddr_in*)i->ifa_netmask)->sin_addr.s_addr);
        found=1;
        break;
    }
    freeifaddrs(all);
    return found?0:-1;
#else
    SceNetCtlInfo address,netmask;
    struct in_addr a,m;
    if(sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS,&address)<0||sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_NETMASK,&netmask)<0)return -1;
    if(inet_pton(AF_INET,address.ip_address,&a)!=1||inet_pton(AF_INET,netmask.netmask,&m)!=1)return -1;
    *ip=ntohl(a.s_addr);
    *mask=ntohl(m.s_addr);
    return 0;
#endif
}
static void ip_text(uint32_t address,char*out,size_t cap) {
    struct in_addr a= {
        htonl(address)
    };
    inet_ntop(AF_INET,&a,out,cap);
}
static void stop_scan(Network*n) {
    for(int i=0;i<24;i++)if(n->probes[i].fd>=0) {
        close(n->probes[i].fd);
        n->probes[i].fd=-1;
    }
    n->scanning=0;
}
static void download_cleanup(Network*n,int remove_partial) {
    if(n->local_fd>=0) {
        sceIoClose(n->local_fd);
        n->local_fd=-1;
    }
    if(remove_partial&&n->partial)sceIoRemove(n->partial_file);
    n->partial=n->download_active=0;
}
static void disconnect(Network*n) {
    /* Destroy invokes pending callbacks with failure; mark download inactive first. */  n->download_active=0;
    n->upload_active=0;
    if(n->upload_fd>=0){sceIoClose(n->upload_fd);n->upload_fd=-1;}
    if(n->smb)smb2_destroy_context(n->smb);
    n->smb=NULL;
    n->file=NULL;
    n->upload_active=0;
    if(n->upload_fd>=0){sceIoClose(n->upload_fd);n->upload_fd=-1;}
    n->busy=n->connected=0;
    download_cleanup(n,1);
    memset(n->password,0,sizeof(n->password));
}
static void error(Network*n,const char*prefix) {
    snprintf(n->message,sizeof(n->message),"%s: %.170s",prefix,n->smb?smb2_get_error(n->smb):"connessione non disponibile");
    n->busy=0;
}
static void start_scan(Network*n) {
    if(n->busy) {
        dm_status("Annulla prima l'operazione SMB");
        return;
    }
    stop_scan(n);
    uint32_t mask;
    if(local_subnet(&n->local,&mask)<0) {
        strcpy(n->message,"Rete IPv4 non disponibile: connetti il Wi-Fi");
        return;
    }
    uint32_t inverse=~mask;
    if(!inverse||(inverse&(inverse+1))) {
        strcpy(n->message,"Maschera di rete non valida");
        return;
    }
    if(inverse>255)mask=0xffffff00u;
    n->first=(n->local&mask)+1;
    n->last=(n->local|~mask)-1;
    if(n->last<n->first) {
        strcpy(n->message,"Sottorete senza indirizzi da scandire");
        return;
    }
    n->next=n->first;
    n->next_port=n->tested=n->count=n->offset=n->mode=0;
    n->scanning=1;
    char first[32],last[32];
    ip_text(n->first,first,sizeof(first));
    ip_text(n->last,last,sizeof(last));
    snprintf(n->message,sizeof(n->message),"Scansione %s - %s%s",first,last,inverse>255?" (segmento /24)":"");
}
static void found(Network*n,uint32_t ip,int port,int open) {
    char address[32];
    ip_text(ip,address,sizeof(address));
    int row=-1;
    for(int i=0;i<n->count;i++)if(!strcmp(n->rows[i].name,address))row=i;
    if(row<0&&n->count<MAX_ROWS) {
        row=n->count++;
        memset(&n->rows[row],0,sizeof(n->rows[row]));
        strcpy(n->rows[row].name,address);
    }
    if(row>=0&&port==445&&open)n->rows[row].smb=1;
}
static void scan_tick(Network*n) {
    uint64_t now=dm_clock_ms();
    for(int i=0;i<24;i++) {
        Probe*p=&n->probes[i];
        if(p->fd<0)continue;
        struct pollfd pollfd= {
            p->fd,POLLOUT,0
        };
        int rc=poll(&pollfd,1,0);
        if(rc>0&&pollfd.revents) {
            int result=0;
            socklen_t size=sizeof(result);
            if(getsockopt(p->fd,SOL_SOCKET,SO_ERROR,&result,&size)==0&&(result==0||result==ECONNREFUSED))found(n,p->ip,p->port,result==0);
            close(p->fd);
            p->fd=-1;
            n->tested++;
        }  else if(now>=p->deadline) {
            close(p->fd);
            p->fd=-1;
            n->tested++;
        }
    }
    for(int i=0;i<24&&n->next<=n->last;i++) {
        Probe*p=&n->probes[i];
        if(p->fd>=0)continue;
        if(n->next==n->local) {
            n->next++;
            n->next_port=0;
            if(n->next>n->last)break;
        }
        p->ip=n->next;
        p->port=probe_ports[n->next_port++];
        if(n->next_port==5) {
            n->next_port=0;
            n->next++;
        }
        p->fd=socket(AF_INET,SOCK_STREAM,0);
        if(p->fd<0) {
            n->tested++;
            continue;
        }
        if(fcntl(p->fd,F_SETFL,O_NONBLOCK)<0) {
            close(p->fd);
            p->fd=-1;
            n->tested++;
            continue;
        }
        struct sockaddr_in address;
        memset(&address,0,sizeof(address));
        address.sin_family=AF_INET;
        address.sin_addr.s_addr=htonl(p->ip);
        address.sin_port=htons(p->port);
        int rc=connect(p->fd,(struct sockaddr*)&address,sizeof(address));
        if(rc==0||errno==ECONNREFUSED) {
            found(n,p->ip,p->port,rc==0);
            close(p->fd);
            p->fd=-1;
            n->tested++;
        }    else if(errno!=EINPROGRESS&&errno!=EWOULDBLOCK) {
            close(p->fd);
            p->fd=-1;
            n->tested++;
        }    else p->deadline=now+400;
    }
    int pending=0;
    for(int i=0;i<24;i++)pending+=n->probes[i].fd>=0;
    if(n->next>n->last&&!pending) {
        n->scanning=0;
        snprintf(n->message,sizeof(n->message),"%d dispositivi rispondono. SMB: porta 445. Doppio clic per aprire.",n->count);
    }
}
static void list_directory(Network*n);
static void directory_result(struct smb2_context*smb,int status,void*data,void*ctx) {
    Network*n=ctx;
    n->busy=0;
    if(status<0) {
        error(n,"Cartella non aperta");
        return;
    }
    struct smb2dir*dir=data;
    n->count=n->offset=0;
    n->mode=2;
    n->click_valid=0;
    struct smb2dirent*entry;
    while((entry=smb2_readdir(smb,dir))&&n->count<MAX_ROWS) {
        if(!strcmp(entry->name,".")||!strcmp(entry->name,".."))continue;
        if(strlen(entry->name)>=256)continue;
        NetRow*r=&n->rows[n->count++];
        memset(r,0,sizeof(*r));
        strcpy(r->name,entry->name);
        r->directory=entry->st.smb2_type==SMB2_TYPE_DIRECTORY;
        r->size=entry->st.smb2_size;
    }
    smb2_closedir(smb,dir);
    snprintf(n->message,sizeof(n->message),"%d elementi | doppio clic: cartella | Scarica: salva file su Vita",n->count);
}
static void list_directory(Network*n) {
    n->busy=1;
    if(smb2_opendir_async(n->smb,n->path,directory_result,n)<0)error(n,"Lettura cartella fallita");
}
static void shares_result(struct smb2_context*smb,int status,void*data,void*ctx) {
    Network*n=ctx;
    n->busy=0;
    struct smb2_share_enum_reply*reply=data;
    if(status) {
        if(reply)smb2_free_data(smb,reply);
        error(n,"Condivisioni non elencabili: usa Connetti con IP/nome");
        return;
    }
    n->count=n->offset=0;
    n->mode=1;
    n->click_valid=0;
    for(uint32_t i=0;i<reply->entries_read&&n->count<MAX_ROWS;i++) {
        struct smb2_share_info_1*share=&reply->share_info.info_1[i];
        if((share->type&3)!=0||strlen(share->netname)>=256)continue;
        NetRow*r=&n->rows[n->count++];
        memset(r,0,sizeof(*r));
        strcpy(r->name,share->netname);
        r->directory=1;
    }
    smb2_free_data(smb,reply);
    snprintf(n->message,sizeof(n->message),"%d condivisioni | se richiede accesso, imposta Credenziali",n->count);
}
static void connected(struct smb2_context*smb,int status,void*data,void*ctx) {
    (void)data;
    Network*n=ctx;
    if(status<0) {
        error(n,"Accesso SMB fallito: controlla Credenziali e Connetti");
        return;
    }
    n->connected=1;
    if(!strcmp(n->share,"IPC$")) {
        if(smb2_share_enum_async(smb,SMB2_SHARE_INFO_1,shares_result,n)<0)error(n,"Enumerazione non avviata");
    }   else list_directory(n);
}
static void connect_share(Network*n,const char*server,const char*share) {
    char server_copy[64],share_copy[256],password[128];
    snprintf(server_copy,sizeof(server_copy),"%s",server);
    snprintf(share_copy,sizeof(share_copy),"%s",share);
    snprintf(password,sizeof(password),"%s",n->password);
    stop_scan(n);
    disconnect(n);
    snprintf(n->password,sizeof(n->password),"%s",password);
    memset(password,0,sizeof(password));
    strcpy(n->server,server_copy);
    strcpy(n->share,share_copy);
    n->path[0]=0;
    n->smb=smb2_init_context();
    if(!n->smb) {
        strcpy(n->message,"Memoria SMB insufficiente");
        return;
    }
    smb2_set_timeout(n->smb,10);
    smb2_set_security_mode(n->smb,SMB2_NEGOTIATE_SIGNING_ENABLED);
    smb2_set_user(n->smb,n->user[0]?n->user:"guest");
    smb2_set_password(n->smb,n->password);
    n->busy=1;
    strcpy(n->message,"Connessione SMB2/3...");
    if(smb2_connect_share_async(n->smb,n->server,n->share,n->user[0]?n->user:"guest",connected,n)<0)error(n,"Connessione non avviata");
}
static void manual(const char*text,void*ctx) {
    DmWindow*w=ctx;
    if(!w->used)return;
    Network*n=w->state;
    char address[512];
    snprintf(address,sizeof(address),"%s",text);
    char*share=strchr(address,'/');
    if(share)*share++=0;
    struct in_addr ip;
    if(inet_pton(AF_INET,address,&ip)!=1||!address[0]||(share&&(!*share||strchr(share,'/')||strlen(share)>=256))) {
        dm_status("Usa IPv4 oppure IPv4/condivisione");
        return;
    }
    connect_share(n,address,share?share:"IPC$");
}
static void password_result(const char*text,void*ctx) {
    DmWindow*w=ctx;
    if(!w->used)return;
    Network*n=w->state;
    if(strlen(text)>=sizeof(n->password)) {
        dm_status("Password troppo lunga (massimo 127 byte)");
        return;
    }
    snprintf(n->password,sizeof(n->password),"%s",text);
    dm_status("Credenziali impostate in memoria: premi Connetti o riapri il PC");
}
static void user_result(const char*text,void*ctx) {
    DmWindow*w=ctx;
    if(!w->used)return;
    Network*n=w->state;
    if(strlen(text)>=sizeof(n->user)) {
        dm_status("Utente troppo lungo (massimo 127 byte)");
        return;
    }
    snprintf(n->user,sizeof(n->user),"%s",text);
    dm_prompt("Password SMB (non salvata)","",password_result,w);
}
static void read_next(Network*n);
static void remote_closed(struct smb2_context*smb,int status,void*data,void*ctx) {
    (void)smb;
    (void)status;
    (void)data;
    Network*n=ctx;
    n->file=NULL;
    n->busy=0;
}
static void commit_download(Network*n) {
    int rc=sceIoClose(n->local_fd);
    n->local_fd=-1;
    if(rc<0) {
        download_cleanup(n,1);
        strcpy(n->message,"Scrittura non completata");
    }    else {
        char backup[1024];
        int backed_up=0;
        SceIoStat stat;
        int existing=sceIoGetstat(n->local_file,&stat)>=0;
        int commit=0;
        if(existing&&!n->replace)commit=-1;
        if(existing&&n->replace) {
            if(snprintf(backup,sizeof(backup),"%s.backup",n->partial_file)>=(int)sizeof(backup)||sceIoGetstat(backup,&stat)>=0||sceIoRename(n->local_file,backup)<0)commit=-1;
            else backed_up=1;
        }
        if(!commit&&sceIoRename(n->partial_file,n->local_file)<0)commit=-1;
        if(commit<0) {
            if(backed_up&&sceIoRename(backup,n->local_file)<0)dm_status("Ripristino fallito: originale conservato nel file .backup");
            download_cleanup(n,1);
            strcpy(n->message,"Salvataggio non completato: originale conservato");
        }  else {
            if(backed_up)sceIoRemove(backup);
            n->partial=n->download_active=0;
            strcpy(n->message,"File scaricato: aperto con l'app associata");
            dm_files_changed();
            dm_open_file(n->local_file);
        }
    }
}
static void read_result(struct smb2_context*smb,int status,void*data,void*ctx) {
    (void)data;
    Network*n=ctx;
    if(!n->download_active)return;
    if(status<0) {
        error(n,"Scaricamento fallito");
        download_cleanup(n,1);
        if(n->file)smb2_close_async(smb,n->file,remote_closed,n);
        return;
    }
    if(status==0) {
        commit_download(n);
        smb2_close_async(smb,n->file,remote_closed,n);
        return;
    }
    int offset=0;
    while(offset<status) {
        int wrote=sceIoWrite(n->local_fd,n->buffer+offset,status-offset);
        if(wrote<=0) {
            error(n,"Scrittura locale fallita");
            download_cleanup(n,1);
            smb2_close_async(smb,n->file,remote_closed,n);
            return;
        }
        offset+=wrote;
    }
    n->downloaded+=status;
    read_next(n);
}
static void read_next(Network*n) {
    if(smb2_pread_async(n->smb,n->file,n->buffer,sizeof(n->buffer),n->downloaded,read_result,n)<0) {
        error(n,"Lettura SMB non avviata");
        download_cleanup(n,1);
        smb2_close_async(n->smb,n->file,remote_closed,n);
    }
}
static void remote_opened(struct smb2_context*smb,int status,void*data,void*ctx) {
    (void)smb;
    Network*n=ctx;
    if(status<0) {
        error(n,"File remoto non aperto");
        download_cleanup(n,1);
        return;
    }
    n->file=data;
    read_next(n);
}
static void destination(const char*path,int replace,void*ctx) {
    DmWindow*w=ctx;
    if(!w->used)return;
    Network*n=w->state;
    if(!n->connected||n->busy)return;
    /* Download to an exclusive sibling first; failed transfers preserve the original. */     if(!dm_fs_writable(path)||strlen(path)>900) {
        dm_status("Destinazione non valida");
        return;
    }
    snprintf(n->local_file,sizeof(n->local_file),"%s",path);
    n->replace=replace;
    n->local_fd=-1;
    static unsigned sequence;
    for(int i=0;i<100&&n->local_fd<0;i++) {
        snprintf(n->partial_file,sizeof(n->partial_file),"%.900s.download-%llu-%u",path,(unsigned long long)dm_clock_ms(),++sequence);
        n->local_fd=sceIoOpen(n->partial_file,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_EXCL,0666);
    }
    if(n->local_fd<0) {
        dm_status("Destinazione non scrivibile");
        return;
    }
    n->partial=1;
    n->downloaded=0;
    n->download_active=n->busy=1;
    if(smb2_open_async(n->smb,n->remote_file,O_RDONLY,remote_opened,n)<0) {
        error(n,"Apertura SMB non avviata");
        download_cleanup(n,1);
    }
}
static void download(DmWindow*w) {
    Network*n=w->state;
    if(n->mode!=2||n->selected>=n->count||n->busy||!n->connected)return;
    NetRow*r=&n->rows[n->selected];
    if(r->directory) {
        dm_status("Apri la cartella con doppio clic");
        return;
    }
    if(dm_fs_join(n->remote_file,sizeof(n->remote_file),n->path,r->name)<0)return;
    n->download_size=r->size;
    DmFileDialogOptions o= {
        DM_FILE_SAVE,"Scarica file dalla rete","ux0:/data/desktop-mode/Desktop/",r->name,0
    };
    dm_file_dialog(&o,destination,w);
}
static void upload_chosen(const char*path,int replace,void*ctx);
static void upload(DmWindow*w){
    Network*n=w->state;
    if(n->mode!=2||!n->connected||n->busy){dm_status("Apri prima una cartella SMB scrivibile");return;}
    DmFileDialogOptions o={DM_FILE_OPEN,"Invia file alla condivisione SMB","ux0:/",NULL,NULL};
    if(dm_file_dialog(&o,upload_chosen,w)<0)dm_status("Selettore file gia aperto");
}
static void open_network(DmWindow*w,const char*arg) {
    (void)arg;
    Network*n=w->state;
    n->selected=-1;
    for(int i=0;i<24;i++)n->probes[i].fd=-1;
    n->local_fd=-1;
    n->upload_fd=-1;
    DmSystemInfo info;
    dm_system_info(&info);
    strcpy(n->message,"Scansiona cerca IPv4 nella rete locale. Connetti apre IP/condivisione SMB.");
}
static void tick(DmWindow*w,unsigned elapsed) {
    (void)elapsed;
    Network*n=w->state;
    if(n->scanning)scan_tick(n);
    if(n->smb) {
        struct pollfd fd= {
            smb2_get_fd(n->smb),smb2_which_events(n->smb),0
        };
        int result=poll(&fd,1,0);
        if(result>=0&&smb2_service(n->smb,fd.revents)<0) {
            int interrupted_upload=n->upload_active;
            error(n,"Connessione SMB interrotta");
            if(interrupted_upload)strncat(n->message,"; verifica eventuale temporaneo remoto",sizeof(n->message)-strlen(n->message)-1);
            disconnect(n);
        }
    }
}
static void remote_renamed(struct smb2_context*smb,int status,void*data,void*ctx){
    (void)smb;(void)data;Network*n=ctx;n->busy=0;
    if(status<0)error(n,"Rinomina fallita (nome esistente o permessi)");else list_directory(n);
}
static void uploaded_removed(struct smb2_context*smb,int status,void*data,void*ctx){
    (void)smb;(void)data;Network*n=ctx;if(!n->upload_active)return;n->busy=0;n->upload_active=0;n->file=NULL;n->upload_temp_created=0;
    if(status<0){char previous[144];snprintf(previous,sizeof(previous),"%.143s",n->message);snprintf(n->message,sizeof(n->message),"%.110s | temp: %.50s",previous,strrchr(n->upload_temp,'/')?strrchr(n->upload_temp,'/')+1:n->upload_temp);}
}
static void uploaded_closed_after_error(struct smb2_context*smb,int status,void*data,void*ctx){
    (void)status;(void)data;Network*n=ctx;if(!n->upload_active)return;n->file=NULL;
    if(n->upload_fd>=0){sceIoClose(n->upload_fd);n->upload_fd=-1;}
    if(smb2_unlink_async(smb,n->upload_temp,uploaded_removed,n)<0){n->busy=0;n->upload_active=0;}
}
static void upload_fail(Network*n,const char*message){
    snprintf(n->message,sizeof(n->message),"%s",message);
    if(n->upload_fd>=0){sceIoClose(n->upload_fd);n->upload_fd=-1;}
    if(n->file){if(smb2_close_async(n->smb,n->file,uploaded_closed_after_error,n)<0){n->file=NULL;if(n->upload_temp_created&&smb2_unlink_async(n->smb,n->upload_temp,uploaded_removed,n)==0)return;n->busy=0;n->upload_active=0;}}
    else if(n->smb&&n->upload_temp_created){if(smb2_unlink_async(n->smb,n->upload_temp,uploaded_removed,n)==0)return;n->busy=0;n->upload_active=0;}
    else{n->busy=0;n->upload_active=0;}
}
static void upload_renamed(struct smb2_context*smb,int status,void*data,void*ctx){
    (void)data;Network*n=ctx;if(!n->upload_active)return;
    if(status<0){upload_fail(n,"Invio completato ma pubblicazione fallita: il file remoto esiste gia o non e rinominabile");return;}
    n->busy=n->upload_active=0;n->file=NULL;n->upload_temp_created=0;
    snprintf(n->message,sizeof(n->message),"File inviato: %.220s",strrchr(n->upload_target,'/')?strrchr(n->upload_target,'/')+1:n->upload_target);
    list_directory(n);
    (void)smb;
}
static void upload_closed(struct smb2_context*smb,int status,void*data,void*ctx){
    (void)data;Network*n=ctx;if(!n->upload_active)return;n->file=NULL;
    if(status<0){upload_fail(n,"Chiusura del file remoto fallita");return;}
    if(smb2_rename_async(smb,n->upload_temp,n->upload_target,upload_renamed,n)<0)upload_fail(n,"Pubblicazione del file remoto non avviata");
}
static void upload_advance(Network*n);
static void upload_write_result(struct smb2_context*smb,int status,void*data,void*ctx){
    (void)smb;(void)data;Network*n=ctx;if(!n->upload_active)return;
    if(status<=0||status>n->upload_chunk-n->upload_chunk_written){upload_fail(n,"Scrittura della condivisione fallita");return;}
    n->upload_chunk_written+=status;n->uploaded+=(unsigned)status;
    if(n->upload_chunk_written<n->upload_chunk){
        if(smb2_pwrite_async(n->smb,n->file,n->buffer+n->upload_chunk_written,n->upload_chunk-n->upload_chunk_written,n->uploaded,upload_write_result,n)<0)upload_fail(n,"Continuazione dell'invio non avviata");
        return;
    }
    upload_advance(n);
}
static void upload_advance(Network*n){
    if(n->uploaded>=n->upload_size){if(smb2_close_async(n->smb,n->file,upload_closed,n)<0)upload_fail(n,"Chiusura del file remoto non avviata");return;}
    int amount=sceIoRead(n->upload_fd,n->buffer,sizeof(n->buffer));
    if(amount<=0){upload_fail(n,"Lettura del file locale interrotta");return;}
    n->upload_chunk=amount;n->upload_chunk_written=0;
    if(smb2_pwrite_async(n->smb,n->file,n->buffer,(uint32_t)amount,n->uploaded,upload_write_result,n)<0)upload_fail(n,"Scrittura SMB non avviata");
}
static void upload_opened(struct smb2_context*smb,int status,void*data,void*ctx){
    Network*n=ctx;if(!n->upload_active)return;
    if(status<0){upload_fail(n,"Impossibile creare un file temporaneo nella condivisione");return;}
    n->file=data;n->upload_temp_created=1;upload_advance(n);(void)smb;
}
static void upload_chosen(const char*path,int replace,void*ctx){
    (void)replace;DmWindow*w=ctx;if(!w->used||!path)return;Network*n=w->state;
    if(!n->connected||!n->smb||n->busy){dm_status("Connetti prima una condivisione SMB");return;}
    SceIoStat st;if(sceIoGetstat(path,&st)<0||SCE_S_ISDIR(st.st_mode)){dm_status("Scegli un file locale leggibile");return;}
    const char*base=strrchr(path,'/');base=base?base+1:path;
    if(dm_fs_join(n->upload_target,sizeof(n->upload_target),n->path,base)<0){dm_status("Nome remoto non valido");return;}
    static unsigned sequence;
    char temporary[256];snprintf(temporary,sizeof(temporary),".desktop-mode-%llx-%x.tmp",(unsigned long long)dm_clock_ms(),++sequence);
    if(dm_fs_join(n->upload_temp,sizeof(n->upload_temp),n->path,temporary)<0){dm_status("Percorso temporaneo remoto troppo lungo");return;}
    n->upload_fd=sceIoOpen(path,SCE_O_RDONLY,0);if(n->upload_fd<0){dm_status("File locale non apribile");return;}
    n->upload_temp_created=0;
    snprintf(n->upload_local,sizeof(n->upload_local),"%s",path);n->upload_size=st.st_size;n->uploaded=0;n->upload_active=n->busy=1;
    snprintf(n->message,sizeof(n->message),"Preparazione invio: %s",base);
    if(smb2_open_async(n->smb,n->upload_temp,O_WRONLY|O_CREAT|O_EXCL,upload_opened,n)<0)upload_fail(n,"Creazione del temporaneo SMB non avviata");
}
static void delete_remote_result(struct smb2_context*smb,int status,void*data,void*ctx){
    (void)smb;(void)data;Network*n=ctx;n->busy=0;if(status<0)error(n,"Eliminazione SMB fallita: controlla i permessi e che la cartella sia vuota");else list_directory(n);
}
static void delete_remote_confirm(int yes,void*ctx){
    DmWindow*w=ctx;if(!yes||!w->used)return;Network*n=w->state;
    if(!n->connected||n->busy||n->selected<0||n->selected>=n->count)return;
    NetRow*r=&n->rows[n->selected];if(dm_fs_join(n->remote_file,sizeof(n->remote_file),n->path,r->name)<0)return;
    n->busy=1;int rc=r->directory?smb2_rmdir_async(n->smb,n->remote_file,delete_remote_result,n):smb2_unlink_async(n->smb,n->remote_file,delete_remote_result,n);
    if(rc<0){n->busy=0;error(n,"Eliminazione SMB non avviata");}
}
static void delete_remote(DmWindow*w){
    Network*n=w->state;if(n->mode!=2||!n->connected||n->busy||n->selected<0||n->selected>=n->count){dm_status("Seleziona prima un file nella condivisione");return;}
    char question[180];snprintf(question,sizeof(question),"Eliminare definitivamente %.120s dal server SMB?",n->rows[n->selected].name);
    dm_confirm("Elimina dalla rete",question,delete_remote_confirm,w);
}
static void created_directory(struct smb2_context*smb,int status,void*data,void*ctx){
    (void)smb;(void)data;Network*n=ctx;n->busy=0;if(status<0)error(n,"Creazione cartella SMB fallita");else list_directory(n);
}
static void new_directory_name(const char*name,void*ctx){
    DmWindow*w=ctx;if(!name||!w->used)return;Network*n=w->state;
    if(!n->connected||n->busy||n->mode!=2){dm_status("Apri prima una cartella SMB");return;}
    char path[1024];if(dm_fs_join(path,sizeof(path),n->path,name)<0){dm_status("Nome cartella non valido");return;}
    n->busy=1;if(smb2_mkdir_async(n->smb,path,created_directory,n)<0){n->busy=0;error(n,"Creazione cartella SMB non avviata");}
}
static void make_directory(DmWindow*w){
    Network*n=w->state;if(!n->connected||n->busy||n->mode!=2){dm_status("Apri prima una cartella SMB");return;}
    dm_prompt("Nuova cartella nella condivisione", "Nuova cartella",new_directory_name,w);
}
static void remote_name(const char*name,void*ctx){
    DmWindow*w=ctx;if(!w->used)return;Network*n=w->state;
    if(!n->connected||!n->smb||n->busy)return;
    if(dm_fs_join(n->rename_target,sizeof(n->rename_target),n->path,name)<0){strcpy(n->message,"Nome non valido");return;}
    if(!strcmp(n->rename_source,n->rename_target))return;
    n->busy=1;
    if(smb2_rename_async(n->smb,n->rename_source,n->rename_target,remote_renamed,n)<0){n->busy=0;error(n,"Rinomina fallita");}
}
static void rename_remote(DmWindow*w){
    Network*n=w->state;
    if(n->mode!=2||!n->connected||n->busy||n->selected<0||n->selected>=n->count){strcpy(n->message,"Seleziona un file o una cartella nella condivisione SMB");return;}
    NetRow*r=&n->rows[n->selected];
    if(!dm_fs_join(n->rename_source,sizeof(n->rename_source),n->path,r->name))dm_prompt("Rinomina nella condivisione SMB",r->name,remote_name,w);
}
static void draw(DmWindow*w) {
    Network*n=w->state;
    const char*buttons[]={dm_localize("Scans.","Scan","Buscar"),dm_localize("Connetti","Connect","Conectar"),dm_localize("Utente","User","Usuario"),dm_localize("Su","Up","Arriba"),dm_localize("Scarica","Download","Descargar"),dm_localize("Invia","Upload","Enviar"),dm_localize("Elimina","Delete","Eliminar"),dm_localize("Rinomina","Rename","Renombrar"),dm_localize("Cartella","Folder","Carpeta"),dm_localize("Stop","Stop","Parar")};
    for(int i=0;i<10;i++) {
        dm_rect(w->x+8+i*69,w->y+36,67,30,DM_COLOR(208,222,237,255));
        dm_text(w->x+10+i*69,w->y+57,buttons[i],INK);
    }
    char address[90];
    snprintf(address,sizeof(address),"%.30s / %.25s / %.20s",n->server,n->share,n->path);
    if(n->mode)dm_text_raw(w->x+20,w->y+92,address,INK);else dm_text(w->x+20,w->y+92,"Dispositivi IPv4 rilevati (non tutti sono PC)",INK);
    int visible=(w->h-175)/28;
    for(int row=0;row<visible&&n->offset+row<n->count;row++) {
        int index=n->offset+row;
        NetRow*r=&n->rows[index];
        char text[100];
        if(index==n->selected)dm_rect(w->x+15,w->y+110+row*28,w->w-30,26,DM_COLOR(151,195,246,255));
        if(n->mode==0)snprintf(text,sizeof(text),"%.60s %s",r->name,r->smb?"[SMB 445]":"");
        else snprintf(text,sizeof(text),"%s %.60s %llu byte",r->directory?"[Cartella]":"[File]",r->name,(unsigned long long)r->size);
        dm_text_raw(w->x+23,w->y+130+row*28,text,INK);
    }
    if(n->scanning) {
        int total=(n->last-n->first+1-(n->local>=n->first&&n->local<=n->last))*5;
        if(total<1)total=1;
        dm_rect(w->x+20,w->y+w->h-53,w->w-40,5,DM_COLOR(208,222,237,255));
        dm_rect(w->x+20,w->y+w->h-53,(w->w-40)*n->tested/total,5,DM_COLOR(55,137,220,255));
    }
    if(n->download_active) {
        char progress[100];
        snprintf(progress,sizeof(progress),"Scaricamento: %llu / %llu byte",(unsigned long long)n->downloaded,(unsigned long long)n->download_size);
        dm_text(w->x+20,w->y+w->h-45,progress,INK);
    }
    if(n->upload_active){char progress[112];snprintf(progress,sizeof(progress),"Invio SMB: %llu / %llu byte",(unsigned long long)n->uploaded,(unsigned long long)n->upload_size);dm_text(w->x+20,w->y+w->h-45,progress,INK);}
    char message[100];
    snprintf(message,sizeof(message),"%.95s",n->message);
    dm_text_raw(w->x+20,w->y+w->h-20,message,INK);
}
static void activate(Network*n) {
    if(n->selected>=n->count||n->busy||n->scanning)return;
    if(n->mode==2&&!n->connected) {
        dm_status("Riconnetti prima la condivisione");
        return;
    }
    NetRow*r=&n->rows[n->selected];
    if(n->mode==0) {
        connect_share(n,r->name,"IPC$");
        return;
    }
    if(n->mode==1) {
        connect_share(n,n->server,r->name);
        return;
    }
    if(r->directory) {
        char path[1024];
        if(dm_fs_join(path,sizeof(path),n->path,r->name)==0) {
            strcpy(n->path,path);
            list_directory(n);
        }
    }
}
static void click(DmWindow*w,int x,int y) {
    Network*n=w->state;
    if(y>=36&&y<66&&x>=8&&x<698) {
        int button=(x-8)/69;
        if(button==0)start_scan(n);
        if(button==1&&!n->busy)dm_prompt("Rete - IPv4/condivisione",n->server,manual,w);
        if(button==2&&!n->busy)dm_prompt("Utente SMB (vuoto: guest)",n->user,user_result,w);
        if(button==3&&!n->busy&&n->connected&&n->mode==2) {
            if(!n->path[0])connect_share(n,n->server,"IPC$");
            else {
                char*end=strrchr(n->path,'/');
                if(end)*end=0;
                else n->path[0]=0;
                list_directory(n);
            }
        }
        if(button==4)download(w);
        if(button==5)upload(w);
        if(button==6)delete_remote(w);
        if(button==7)rename_remote(w);
        if(button==8)make_directory(w);
        if(button==9) {
            int interrupted_upload=n->upload_active;
            stop_scan(n);
            disconnect(n);
            strcpy(n->message,interrupted_upload?"Invio interrotto; controlla il temporaneo sul server. Password rimossa.":"Operazione annullata; password rimossa dalla memoria");
        }
        return;
    }
    if(y>=110&&y<w->h-65&&x>=15&&x<w->w-15) {
        int index=n->offset+(y-110)/28;
        if(index>=n->count)return;
        n->selected=index;
        uint64_t now=dm_clock_ms();
        int twice=n->click_valid&&index==n->last_row&&now-n->clicked<450;
        n->last_row=index;
        n->clicked=now;
        n->click_valid=!twice;
        if(twice)activate(n);
    }
}
static void key(DmWindow*w,int k) {
    Network*n=w->state;
    if(k==DM_KEY_UP&&n->offset)n->offset--;
    if(k==DM_KEY_DOWN&&n->offset+(w->h-175)/28<n->count)n->offset++;
    if(k==DM_KEY_ENTER)activate(n);
}
static int close_network(DmWindow*w) {
    Network*n=w->state;
    int interrupted_upload=n->upload_active;
    stop_scan(n);
    disconnect(n);
    if(interrupted_upload)dm_status("Invio interrotto; controlla il temporaneo sul server SMB.");
    return 1;
}
const DmApp dm_network_app= {
    DM_API_VERSION,"network","Rete locale / SMB",sizeof(Network),open_network,draw,click,0,key,close_network,0,tick
};

/* Native networking is a host service for the sandboxed network UI. Each WASM
 * window gets a private native state object, and callbacks are invalidated when
 * that window closes or its module traps. */
#define NETWORK_SERVICE_LIMIT DM_MAX_WINDOWS
typedef struct { DmWindow *owner; DmWindow proxy; Network *state; } NetworkService;
static NetworkService services[NETWORK_SERVICE_LIMIT];
static NetworkService *service_for(DmWindow *owner) {
    for(int i=0;i<NETWORK_SERVICE_LIMIT;i++)if(services[i].owner==owner&&services[i].state)return &services[i];
    return NULL;
}
int dm_network_service_open(DmWindow *owner) {
    if(!owner||service_for(owner))return -1;
    NetworkService *service=NULL;
    for(int i=0;i<NETWORK_SERVICE_LIMIT;i++)if(!services[i].state){service=&services[i];break;}
    if(!service)return -1;
    memset(service,0,sizeof(*service));service->state=calloc(1,sizeof(Network));
    if(!service->state)return -1;
    service->owner=owner;service->proxy=*owner;service->proxy.app=&dm_network_app;service->proxy.state=service->state;service->proxy.used=1;
    open_network(&service->proxy,NULL);return 0;
}
void dm_network_service_close(DmWindow *owner) {
    NetworkService *service=service_for(owner);if(!service)return;
    service->proxy.used=0;close_network(&service->proxy);free(service->state);memset(service,0,sizeof(*service));
}
void dm_network_service_tick(DmWindow *owner,unsigned elapsed_ms) {
    NetworkService *service=service_for(owner);if(!service)return;
    service->proxy.x=owner->x;service->proxy.y=owner->y;service->proxy.w=owner->w;service->proxy.h=owner->h;
    tick(&service->proxy,elapsed_ms);
}
int dm_network_service_action(DmWindow *owner,int action) {
    NetworkService *service=service_for(owner);if(!service||action<0||action>9)return -1;
    service->proxy.x=owner->x;service->proxy.y=owner->y;service->proxy.w=owner->w;service->proxy.h=owner->h;
    click(&service->proxy,8+action*69+1,40);return 0;
}
int dm_network_service_select(DmWindow *owner,int index,int activate_row) {
    NetworkService *service=service_for(owner);if(!service||index<0||index>=service->state->count)return -1;
    Network *n=service->state;n->selected=index;
    if(activate_row)activate(n);
    return 0;
}
int dm_network_service_info(DmWindow *owner,DmNetworkInfo *info) {
    NetworkService *service=service_for(owner);if(!service||!info)return -1;
    Network*n=service->state;memset(info,0,sizeof(*info));
    snprintf(info->server,sizeof(info->server),"%s",n->server);snprintf(info->share,sizeof(info->share),"%s",n->share);
    snprintf(info->path,sizeof(info->path),"%s",n->path);snprintf(info->message,sizeof(info->message),"%s",n->message);
    info->count=n->count;info->offset=n->offset;info->selected=n->selected<0?UINT32_MAX:(uint32_t)n->selected;info->mode=n->mode;
    info->busy=n->busy;info->scanning=n->scanning;info->connected=n->connected;info->download_active=n->download_active;info->upload_active=n->upload_active;
    info->tested=n->tested;info->scan_total=(uint64_t)(n->last>=n->first?n->last-n->first+1:0)*5;info->downloaded=n->downloaded;info->download_size=n->download_size;info->uploaded=n->uploaded;info->upload_size=n->upload_size;
    return 0;
}
int dm_network_service_row(DmWindow *owner,int index,DmNetworkRow *row) {
    NetworkService *service=service_for(owner);if(!service||!row||index<0||index>=service->state->count)return -1;
    NetRow*r=&service->state->rows[index];memset(row,0,sizeof(*row));snprintf(row->name,sizeof(row->name),"%s",r->name);
    row->directory=r->directory;row->smb=r->smb;row->size=r->size;return 0;
}
#ifdef DESKTOP_PREVIEW
#include <assert.h>
void dm_network_test(const char*root) {
    DmWindow storage={0};storage.used=1;storage.w=700;storage.h=420;storage.app=&dm_network_app;storage.state=calloc(1,sizeof(Network));assert(storage.state);
    DmWindow*w=&storage;Network*n=w->state;open_network(w,NULL);
    found(n,0xc0a80102u,80,1);
    found(n,0xc0a80102u,445,1);
    found(n,0xc0a80103u,445,0);
    assert(n->count==2&&n->rows[0].smb&&!n->rows[1].smb&&!strcmp(n->rows[0].name,"192.168.1.2"));
    manual("file:///etc/passwd",w);
    assert(!n->smb);
    char final[DM_PATH_MAX],temp[DM_PATH_MAX],content[64];
    assert(dm_fs_join(final,sizeof(final),root,"network.txt")==0);
    assert(dm_fs_join(temp,sizeof(temp),root,"network.partial")==0);
    assert(dm_fs_write(final,"original",8,1)==0);
    strcpy(n->local_file,final);
    strcpy(n->partial_file,temp);
    n->local_fd=sceIoOpen(temp,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_EXCL,0666);
    assert(n->local_fd>=0);
    assert(sceIoWrite(n->local_fd,"partial",7)==7);
    n->partial=1;
    download_cleanup(n,1);
    assert(dm_fs_read(final,content,sizeof(content))==8&&!strcmp(content,"original"));
    SceIoStat stat;
    assert(sceIoGetstat(temp,&stat)<0);
    /* A completed download cannot overwrite without explicit picker approval. */  assert(dm_fs_write(temp,"new content",11,1)==0);
    n->local_fd=sceIoOpen(temp,SCE_O_WRONLY,0666);
    n->partial=n->download_active=1;
    n->replace=0;
    commit_download(n);
    assert(dm_fs_read(final,content,sizeof(content))==8&&!strcmp(content,"original"));
    assert(sceIoGetstat(temp,&stat)<0);
    assert(dm_fs_write(temp,"new content",11,1)==0);
    n->local_fd=sceIoOpen(temp,SCE_O_WRONLY,0666);
    n->partial=n->download_active=1;
    n->replace=1;
    commit_download(n);
    assert(dm_fs_read(final,content,sizeof(content))==11&&!strcmp(content,"new content"));
    assert(sceIoGetstat(temp,&stat)<0);
    n->mode=2;
    n->connected=0;
    activate(n);
    assert(!n->busy);
    close_network(w);free(w->state);
    puts("PASS: network address rows/SMB marker, invalid address handling, cancelled/failed download preserves original, confirmed download replacement, disconnected navigation guard (offline)");
}
#endif
