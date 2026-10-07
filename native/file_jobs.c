#include "file_jobs.h"
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#define MAX_DEPTH 33
#define COPY 1
#define EMPTY 2
#define TRASH 3
#define RESTORE 4
typedef struct {
    char source[DM_PATH_MAX],destination[DM_PATH_MAX];
    int phase,dirfd;
}
Frame;
static struct {
    int active,type,preflight,depth,root_index,root_count,in,out;
    uint64_t bytes,total,items,total_items,started;
    char roots[20][DM_PATH_MAX],source[DM_PATH_MAX],destination[DM_PATH_MAX],current[DM_PATH_MAX],partial[DM_PATH_MAX],error[200];
    Frame stack[MAX_DEPTH];
    char copy_buffer[65536];
    DmJobResult callback;
    void*context;
}
job;
static const char*mounts[]= {
    "ux0:","uma0:","imc0:","ur0:","ud0:"
};
int dm_job_active(void) {
    return job.active;
}
void dm_job_progress(DmJobProgress*p) {
    *p=(DmJobProgress) {
        job.active,job.preflight,job.bytes,job.total,job.items,job.total_items
    };
}
int dm_trash_directories(const char*source,char*files,char*info) {
    const char*colon=strchr(source,':');
    if(!colon||colon-source>8)return -1;
    char root[12];
    snprintf(root,sizeof(root),"%.*s",(int)(colon-source+1),source);
    snprintf(files,DM_PATH_MAX,"%s/data/desktop-mode/Trash/files",root);
    snprintf(info,DM_PATH_MAX,"%s/data/desktop-mode/Trash/info",root);
    return dm_fs_writable(files)&&dm_fs_writable(info)?0:-1;
}
int dm_trash_has_items(const char*unit) {
    unsigned count=unit?1:sizeof(mounts)/sizeof(*mounts);
    for(unsigned i=0;i<count;i++) {
        char files[DM_PATH_MAX],info[DM_PATH_MAX];
        if(dm_trash_directories(unit?unit:mounts[i],files,info))continue;
        int fd=sceIoDopen(files);
        if(fd<0)continue;
        SceIoDirent entry;
        int found=0;
        memset(&entry,0,sizeof(entry));
        while(sceIoDread(fd,&entry)>0) {
            if(strcmp(entry.d_name,".")&&strcmp(entry.d_name,"..")) {
                found=1;
                break;
            }
            memset(&entry,0,sizeof(entry));
        }
        sceIoDclose(fd);
        if(found)return 1;
    }
    return 0;
}
static int descendant(const char*source,const char*dest) {
    size_t n=strlen(source);
    while(n&&source[n-1]=='/')n--;
    return !strncasecmp(source,dest,n)&&(dest[n]=='/'||dest[n]==0);
}
static void close_resources(void) {
    if(job.in>=0)sceIoClose(job.in);
    if(job.out>=0)sceIoClose(job.out);
    job.in=job.out=-1;
    for(int i=0;i<=job.depth;i++)if(job.stack[i].dirfd>=0) {
        sceIoDclose(job.stack[i].dirfd);
        job.stack[i].dirfd=-1;
    }
}
static void finish(int success,const char*message) {
    DmJobResult cb=job.callback;
    void*ctx=job.context;
    close_resources();
    if(!success&&job.partial[0])sceIoRemove(job.partial);
    job.active=0;
    dm_status(message);
    if(cb)cb(success,ctx);
}
void dm_job_cancel(void) {
    if(job.active)finish(0,job.type==COPY?"Copia annullata: eventuali copie gia completate restano nella destinazione":"Operazione annullata; gli elementi restanti sono conservati");
}
static int begin(int type,DmJobResult cb,void*ctx) {
    if(job.active)return -1;
    memset(&job,0,sizeof(job));
    job.active=1;
    job.type=type;
    job.preflight=1;
    job.depth=-1;
    job.in=job.out=-1;
    for(int i=0;i<MAX_DEPTH;i++)job.stack[i].dirfd=-1;
    job.callback=cb;
    job.context=ctx;
    job.started=dm_clock_ms();
    return 0;
}
int dm_job_copy(const char*src,const char*dst,DmJobResult cb,void*ctx) {
    if(!src||!src[0]||!dst||!dst[0]||!dm_fs_writable(dst)||descendant(src,dst)||strlen(src)>=DM_PATH_MAX||strlen(dst)>=DM_PATH_MAX)return -1;
    if(begin(COPY,cb,ctx))return -1;
    snprintf(job.source,DM_PATH_MAX,"%s",src);
    snprintf(job.destination,DM_PATH_MAX,"%s",dst);
    snprintf(job.roots[0],DM_PATH_MAX,"%s",src);
    job.root_count=1;
    return 0;
}
int dm_job_trash(const char*src,DmJobResult cb,void*ctx) {
    char files[DM_PATH_MAX],info[DM_PATH_MAX];
    if(!dm_fs_writable(src)||dm_trash_directories(src,files,info)||descendant(src,files)||strstr(src,"/desktop-mode/Trash/")||strlen(src)>=DM_PATH_MAX)return -1;
    if(begin(TRASH,cb,ctx))return -1;
    snprintf(job.source,DM_PATH_MAX,"%s",src);
    job.preflight=0;
    return 0;
}
int dm_job_restore(const char*src,DmJobResult cb,void*ctx) {
    if(!strstr(src,"/desktop-mode/Trash/files/")||strlen(src)>=DM_PATH_MAX)return -1;
    if(begin(RESTORE,cb,ctx))return -1;
    snprintf(job.source,DM_PATH_MAX,"%s",src);
    job.preflight=0;
    return 0;
}
int dm_job_empty_trash_unit(const char*unit,DmJobResult cb,void*ctx) {
    int allowed=0;
    for(unsigned i=0;i<sizeof(mounts)/sizeof(*mounts);i++)if(!strcmp(unit,mounts[i]))allowed=1;
    if(!allowed||begin(EMPTY,cb,ctx))return -1;
    char files[DM_PATH_MAX],info[DM_PATH_MAX];
    if(!dm_trash_directories(unit,files,info)) {
        if(dm_fs_is_directory(files))snprintf(job.roots[job.root_count++],DM_PATH_MAX,"%s",files);
        if(dm_fs_is_directory(info))snprintf(job.roots[job.root_count++],DM_PATH_MAX,"%s",info);
    }
    return 0;
}
int dm_job_empty_trash(DmJobResult cb,void*ctx) {
    if(begin(EMPTY,cb,ctx))return -1;
    for(unsigned i=0;i<sizeof(mounts)/sizeof(*mounts);i++) {
        char files[DM_PATH_MAX],info[DM_PATH_MAX];
        if(dm_trash_directories(mounts[i],files,info))continue;
        if(dm_fs_is_directory(files))snprintf(job.roots[job.root_count++],DM_PATH_MAX,"%s",files);
        if(dm_fs_is_directory(info))snprintf(job.roots[job.root_count++],DM_PATH_MAX,"%s",info);
    }
    return 0;
}
static int setup_trash(const char*source,char*files,char*info) {
    if(dm_trash_directories(source,files,info))return -1;
    char dir[DM_PATH_MAX];
    snprintf(dir,sizeof(dir),"%s",files);
    char*end=strstr(dir,"/data/");
    if(!end)return -1;
    end[5]=0;
    sceIoMkdir(dir,0777);
    snprintf(dir,sizeof(dir),"%s",files);
    end=strstr(dir,"/Trash");
    if(!end)return -1;
    *end=0;
    sceIoMkdir(dir,0777);
    snprintf(dir,sizeof(dir),"%s",files);
    end=strrchr(dir,'/');
    *end=0;
    sceIoMkdir(dir,0777);
    sceIoMkdir(files,0777);
    sceIoMkdir(info,0777);
    return dm_fs_is_directory(files)&&dm_fs_is_directory(info)?0:-1;
}
static int metadata_path(const char*source,char*infofile) {
    char files[DM_PATH_MAX],info[DM_PATH_MAX];
    if(dm_trash_directories(source,files,info))return -1;
    const char*base=strrchr(source,'/');
    if(!base)return -1;
    return dm_fs_join(infofile,DM_PATH_MAX,info,base+1);
}
static void move_to_trash(void) {
    char files[DM_PATH_MAX],info[DM_PATH_MAX],target[DM_PATH_MAX],metadata[DM_PATH_MAX];
    if(setup_trash(job.source,files,info)) {
        finish(0,"Cestino non disponibile su questa unita");
        return;
    }
    const char*base=strrchr(job.source,'/');
    base=base?base+1:job.source;
    char name[256];
    int created=0;
    for(int i=0;i<1000;i++) {
        snprintf(name,sizeof(name),"%llu-%d-%.190s",(unsigned long long)time(NULL),i,base);
        if(dm_fs_join(target,sizeof(target),files,name)||dm_fs_join(metadata,sizeof(metadata),info,name))break;
        SceIoStat existing;
        memset(&existing,0,sizeof(existing));
        if(sceIoGetstat(target,&existing)>=0)continue;
        if(sceIoGetstat(metadata,&existing)>=0)continue;
        if(dm_fs_write(metadata,job.source,strlen(job.source),1)<0) {
            finish(0,"Cestino non disponibile: informazioni non salvate");
            return;
        }
        created=1;
        break;
    }
    if(!created) {
        finish(0,"Impossibile creare le informazioni del Cestino");
        return;
    }
    if(sceIoRename(job.source,target)<0) {
        sceIoRemove(metadata);
        finish(0,"Spostamento nel Cestino fallito; file originale conservato");
        return;
    }
    finish(1,"Elemento spostato nel Cestino");
}
static void restore(void) {
    char metadata[DM_PATH_MAX],origin[DM_PATH_MAX];
    if(metadata_path(job.source,metadata)||dm_fs_read(metadata,origin,sizeof(origin))<0||!dm_fs_writable(origin)) {
        finish(0,"Informazioni di ripristino non disponibili");
        return;
    }
    origin[strcspn(origin,"\r\n")]=0;
    SceIoStat s;
    memset(&s,0,sizeof(s));
    if(sceIoGetstat(origin,&s)>=0) {
        finish(0,"La destinazione originale esiste gia: ripristino annullato");
        return;
    }
    if(sceIoRename(job.source,origin)<0) {
        finish(0,"Ripristino fallito: controlla la cartella originale");
        return;
    }
    sceIoRemove(metadata);
    finish(1,"Elemento ripristinato nella cartella originale");
}
static int push(const char*src,const char*dst) {
    if(job.depth+1>=MAX_DEPTH)return -1;
    Frame*f=&job.stack[++job.depth];
    memset(f,0,sizeof(*f));
    f->dirfd=-1;
    snprintf(f->source,sizeof(f->source),"%s",src);
    snprintf(f->destination,sizeof(f->destination),"%s",dst?dst:"");
    return 0;
}
static void fail(const char*reason) {
    char message[256];
    snprintf(message,sizeof(message),"%s: %.130s%s",reason,job.current,job.type==COPY?" (eventuali copie parziali restano)":"");
    finish(0,message);
}
static void step(void) {
    if(job.depth<0) {
        if(job.root_index>=job.root_count) {
            if(job.preflight) {
                job.preflight=0;
                job.root_index=0;
                job.started=dm_clock_ms();
                return;
            }
            finish(1,job.type==COPY?"Copia completata":"Cestino svuotato");
            return;
        }
        push(job.roots[job.root_index++],job.type==COPY?job.destination:NULL);
        return;
    }
    Frame*f=&job.stack[job.depth];
    snprintf(job.current,sizeof(job.current),"%s",f->source);
    if(f->phase==0) {
        SceIoStat stat;
        memset(&stat,0,sizeof(stat));
        if(sceIoGetstat(f->source,&stat)<0) {
            fail("Elemento non accessibile");
            return;
        }
        if(job.preflight) {
            job.total_items++;
            if(!SCE_S_ISDIR(stat.st_mode)) {
                job.total+=stat.st_size;
                job.depth--;
                return;
            }
        }
        if(SCE_S_ISDIR(stat.st_mode)) {
            if(!job.preflight&&job.type==COPY&&sceIoMkdir(f->destination,0777)<0) {
                fail("Creazione cartella fallita");
                return;
            }
            f->dirfd=sceIoDopen(f->source);
            if(f->dirfd<0) {
                fail("Apertura cartella fallita");
                return;
            }
            f->phase=1;
            return;
        }
        if(job.type==EMPTY) {
            if(sceIoRemove(f->source)<0) {
                fail("Eliminazione fallita");
                return;
            }
            job.bytes+=stat.st_size;
            job.items++;
            job.depth--;
            return;
        }
        job.in=sceIoOpen(f->source,SCE_O_RDONLY,0);
        job.out=sceIoOpen(f->destination,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_EXCL,0666);
        if(job.out>=0)snprintf(job.partial,sizeof(job.partial),"%s",f->destination);
        if(job.in<0||job.out<0) {
            fail("Apertura file per copia fallita");
            return;
        }
        snprintf(job.partial,sizeof(job.partial),"%s",f->destination);
        f->phase=2;
        return;
    }
    if(f->phase==1) {
        SceIoDirent entry;
        memset(&entry,0,sizeof(entry));
        int n=sceIoDread(f->dirfd,&entry);
        if(n<0) {
            fail("Lettura cartella fallita");
            return;
        }
        if(n==0) {
            sceIoDclose(f->dirfd);
            f->dirfd=-1;
            if(!job.preflight) {
                if(job.type==EMPTY&&sceIoRmdir(f->source)<0) {
                    fail("Eliminazione cartella fallita");
                    return;
                }
                job.items++;
            }
            job.depth--;
            return;
        }
        if(!strcmp(entry.d_name,".")||!strcmp(entry.d_name,".."))return;
        char source[DM_PATH_MAX],destination[DM_PATH_MAX];
        if(dm_fs_join(source,sizeof(source),f->source,entry.d_name)||(!job.preflight&&job.type==COPY&&dm_fs_join(destination,sizeof(destination),f->destination,entry.d_name))||push(source,!job.preflight&&job.type==COPY?destination:NULL)) {
            fail("Percorso o profondita oltre il limite");
            return;
        }
        return;
    }
    char*buffer=job.copy_buffer;
    int n=sceIoRead(job.in,buffer,sizeof(job.copy_buffer));
    if(n<0) {
        fail("Lettura file fallita");
        return;
    }
    if(n==0) {
        int error=sceIoClose(job.out);
        sceIoClose(job.in);
        job.in=job.out=-1;
        if(error<0) {
            fail("Chiusura file fallita");
            return;
        }
        job.partial[0]=0;
        job.items++;
        job.depth--;
        return;
    }
    int offset=0;
    while(offset<n) {
        int written=sceIoWrite(job.out,buffer+offset,n-offset);
        if(written<=0) {
            fail("Scrittura file fallita");
            return;
        }
        offset+=written;
    }
    job.bytes+=n;
}
void dm_job_tick(void) {
    if(!job.active)return;
    if(job.type==TRASH) {
        move_to_trash();
        return;
    }
    if(job.type==RESTORE) {
        restore();
        return;
    }
    uint64_t start=dm_clock_ms(),before=job.bytes;
    for(int n=0;n<128&&job.active&&dm_clock_ms()-start<6&&job.bytes-before<262144;n++)step();
}
void dm_job_draw(void) {
    if(!job.active)return;
    dm_rect(0,0,960,544,DM_COLOR(3,12,27,130));
    dm_rect(230,155,500,240,DM_COLOR(236,245,253,255));
    dm_rect(230,155,500,30,DM_COLOR(36,77,140,255));
    dm_text(247,178,job.type==COPY?"Copia dei file":job.type==EMPTY?"Svuotamento Cestino":job.type==TRASH?"Spostamento nel Cestino":"Ripristino",DM_COLOR(255,255,255,255));
    char name[70];
    const char*base=strrchr(job.current,'/');
    snprintf(name,sizeof(name),"%.57s",base?base+1:job.current);
    dm_text(250,217,job.preflight?"Calcolo di dimensione ed elementi...":name,DM_COLOR(24,38,55,255));
    double fraction=job.total?((double)job.bytes/job.total):job.total_items?(double)job.items/job.total_items:0;
    if(fraction>1)fraction=1;
    dm_rect(250,233,460,22,DM_COLOR(187,205,225,255));
    dm_rect(252,235,job.preflight?35+(int)((dm_clock_ms()/50)%400):(int)(456*fraction),18,DM_COLOR(95,184,112,255));
    char line[100];
    snprintf(line,sizeof(line),"%.1f / %.1f MiB | %llu / %llu elementi",job.bytes/1048576.0,job.total/1048576.0,(unsigned long long)job.items,(unsigned long long)job.total_items);
    dm_text(250,284,line,DM_COLOR(24,38,55,255));
    uint64_t elapsed=dm_clock_ms()-job.started;
    if(!job.preflight&&fraction>0&&elapsed>200) {
        unsigned seconds=(unsigned)(elapsed/1000.0*(1/fraction-1)+.999);
        snprintf(line,sizeof(line),"Tempo trascorso: %llu s | rimanente: circa %u s",(unsigned long long)(elapsed/1000),seconds);
    }    else snprintf(line,sizeof(line),"Tempo rimanente: stima in corso...");
    dm_text(250,315,line,DM_COLOR(24,38,55,255));
    dm_rect(525,345,185,32,DM_COLOR(210,224,241,255));
    dm_text_center(617,367,"Annulla",DM_COLOR(24,38,55,255));
}
