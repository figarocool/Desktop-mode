#include "drop_transfer.h"
#include "file_jobs.h"
#include "preferences.h"
extern void dm_files_changed(void);
#include <psp2/io/stat.h>
#include <psp2/io/fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static struct{char(*paths)[DM_PATH_MAX];int count,index,action,choosing,cleanup_sources;char directory[DM_PATH_MAX],target[DM_PATH_MAX];} transfer;
int dm_drop_active(void){return transfer.choosing;}
void dm_drop_cancel(void){if(!transfer.choosing)return;free(transfer.paths);memset(&transfer,0,sizeof(transfer));}
static void advance(void);
static void finish(int success){free(transfer.paths);memset(&transfer,0,sizeof(transfer));dm_files_changed();dm_status(dm_localize(success?"Trasferimento completato":"Trasferimento interrotto; originali non completati conservati",success?"Transfer complete":"Transfer stopped; remaining originals preserved",success?"Transferencia completada":"Transferencia detenida; originales restantes conservados"));}
static void moved(int success,void*ctx){(void)ctx;if(success!=1){finish(0);return;}extern int dm_files_renamed(const char*,const char*);dm_files_renamed(transfer.paths[transfer.index],transfer.target);transfer.index++;advance();}
static void copied(int success,void*ctx){(void)ctx;if(success!=1){finish(0);return;}if(transfer.action==2){if(dm_job_trash(transfer.paths[transfer.index],moved,NULL)<0)finish(0);return;}if(transfer.cleanup_sources)sceIoRemove(transfer.paths[transfer.index]);transfer.index++;advance();}
static void advance(void){
 while(transfer.index<transfer.count){const char*source=transfer.paths[transfer.index];const char*base=strrchr(source,'/');base=base?base+1:source;int available=0;
 for(int i=0;i<100;i++){char name[256];const char*extension=strrchr(base,'.');if(transfer.action==1){if(i)snprintf(name,sizeof(name),"%.200s - %d.dmlink",base,i);else snprintf(name,sizeof(name),"%.220s.dmlink",base);}else if(!i)snprintf(name,sizeof(name),"%.255s",base);else if(extension&&!dm_fs_is_directory(source))snprintf(name,sizeof(name),"%.*s - copia %d%.24s",(int)(extension-base)>190?190:(int)(extension-base),base,i,extension);else snprintf(name,sizeof(name),"%.220s - copia %d",base,i);if(dm_fs_join(transfer.target,sizeof(transfer.target),transfer.directory,name)<0)break;SceIoStat info;if(sceIoGetstat(transfer.target,&info)<0){available=1;break;}}
 if(!available){finish(0);return;}if(transfer.action==1){if(dm_fs_write(transfer.target,source,strlen(source),1)<0){finish(0);return;}transfer.index++;continue;}
 if(transfer.action==2){extern int dm_fs_move(const char*,const char*);const char*a=strchr(source,':'),*b=strchr(transfer.target,':');if(a&&b&&a-source==b-transfer.target&&!strncmp(source,transfer.target,a-source)){if(dm_fs_move(source,transfer.target)<0){finish(0);return;}transfer.index++;continue;}if(!dm_fs_writable(source)){finish(0);return;}}
 if(dm_job_copy(source,transfer.target,copied,NULL)<0)finish(0);
 return;
 }finish(1);
}
static int begin(const char(*paths)[DM_PATH_MAX],int count,const char*directory,int choosing,int cleanup){if(!paths||count<1||count>2048||transfer.paths||dm_job_active()||!dm_fs_writable(directory))return -1;for(int i=0;i<count;i++)if(dm_fs_is_directory(paths[i])){size_t n=strlen(paths[i]);if(!strncmp(paths[i],directory,n)&&(directory[n]==0||directory[n]=='/'))return -1;}transfer.paths=malloc((size_t)count*DM_PATH_MAX);if(!transfer.paths)return -1;memcpy(transfer.paths,paths,(size_t)count*DM_PATH_MAX);transfer.count=count;snprintf(transfer.directory,sizeof(transfer.directory),"%s",directory);transfer.choosing=choosing;transfer.cleanup_sources=cleanup;if(!choosing){transfer.action=0;advance();}return 0;}
int dm_drop_begin(const char(*paths)[DM_PATH_MAX],int count,const char*directory){return begin(paths,count,directory,1,0);}
int dm_drop_copy_begin(const char(*paths)[DM_PATH_MAX],int count,const char*directory,int cleanup){return begin(paths,count,directory,0,cleanup);}
void dm_drop_draw(void){if(!transfer.choosing)return;dm_rect(0,0,960,544,DM_COLOR(5,12,25,180));dm_rect(220,180,520,245,DM_COLOR(234,242,250,255));dm_text(240,215,dm_localize("Come trasferire qui?","How do you want to transfer here?","Como quieres transferir aqui?"),DM_COLOR(24,38,55,255));const char*labels[]={"Copia qui","Crea collegamento qui","Sposta qui","Annulla"};for(int i=0;i<4;i++){dm_rect(240,230+i*43,480,36,DM_COLOR(202,222,243,255));dm_text(250,254+i*43,dm_ui_translate(labels[i]),DM_COLOR(24,38,55,255));}}
int dm_drop_click(int x,int y){if(!transfer.choosing)return 0;if(x>=240&&x<720&&y>=230&&y<402){int action=(y-230)/43;if(action==3){dm_drop_cancel();return 1;}transfer.action=action;transfer.choosing=0;advance();}return 1;}
