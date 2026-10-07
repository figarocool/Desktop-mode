#include "multi_file.h"
#include "file_jobs.h"
#include <psp2/io/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static struct{char(*paths)[DM_PATH_MAX];char directory[DM_PATH_MAX];int count,index,trash;} batch;
static void advance(void);
static void done(int success,void*ctx){(void)ctx;if(success!=1){free(batch.paths);batch.paths=NULL;return;}batch.index++;advance();}
static void advance(void){
 if(batch.index==batch.count){char message[100];snprintf(message,sizeof(message),"%d elementi %s",batch.count,batch.trash?"spostati nel Cestino":"copiati");free(batch.paths);batch.paths=NULL;dm_status(message);return;}
 const char*source=batch.paths[batch.index];int result=-1;
 if(batch.trash)result=dm_job_trash(source,done,NULL);
 else{char name[256],target[DM_PATH_MAX];const char*base=strrchr(source,'/');base=base?base+1:source;
  for(int i=0;i<100;i++){if(!i)snprintf(name,sizeof(name),"%.255s",base);else{const char*ext=strrchr(base,'.');if(ext&&!dm_fs_is_directory(source))snprintf(name,sizeof(name),"%.*s - copia %d%.24s",(int)(ext-base)>190?190:(int)(ext-base),base,i,ext);else snprintf(name,sizeof(name),"%.220s - copia %d",base,i);}if(dm_fs_join(target,sizeof(target),batch.directory,name))break;SceIoStat st;if(sceIoGetstat(target,&st)>=0)continue;result=dm_job_copy(source,target,done,NULL);break;}
 }
 if(result<0){free(batch.paths);batch.paths=NULL;dm_status("Operazione multipla interrotta: percorso protetto o destinazione non valida");}
}
static int begin(const char(*paths)[DM_PATH_MAX],int count,const char*directory,int trash){
 if(!paths||count<1||count>2048||batch.paths||dm_job_active())return -1;
 if(!trash&&(!directory||!dm_fs_writable(directory)))return -1;
 batch.paths=malloc((size_t)count*DM_PATH_MAX);if(!batch.paths)return -1;memcpy(batch.paths,paths,(size_t)count*DM_PATH_MAX);batch.count=count;batch.index=0;batch.trash=trash;if(directory)snprintf(batch.directory,sizeof(batch.directory),"%s",directory);advance();return batch.paths?0:-1;
}
int dm_multi_copy(const char(*paths)[DM_PATH_MAX],int count,const char*directory){return begin(paths,count,directory,0);}
int dm_multi_trash(const char(*paths)[DM_PATH_MAX],int count){return begin(paths,count,NULL,1);}
