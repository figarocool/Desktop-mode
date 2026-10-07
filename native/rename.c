#include "rename.h"
#include <stdio.h>
#include <string.h>
static char rename_source[DM_PATH_MAX];
void dm_path_renamed(char*path,size_t capacity,const char*source,const char*destination){
 size_t n=strlen(source);if(strncmp(path,source,n)||(path[n]&&path[n]!='/'))return;
 char result[DM_PATH_MAX];int length=snprintf(result,sizeof(result),"%s%s",destination,path+n);
 if(length>=0&&(size_t)length<capacity&&(size_t)length<sizeof(result))snprintf(path,capacity,"%s",result);
}
static void renamed(const char*name,void*ctx){
 (void)ctx;char directory[DM_PATH_MAX],target[DM_PATH_MAX];snprintf(directory,sizeof(directory),"%s",rename_source);dm_fs_parent(directory);
 if(dm_fs_join(target,sizeof(target),directory,name)<0){dm_status("Nome non valido: evita /, \\, : e nomi vuoti");return;}
 if(dm_fs_rename(rename_source,target)<0)dm_status("Rinomina fallita: nome esistente, elemento in uso o cartella non scrivibile");
 else dm_status("Elemento rinominato");
}
int dm_rename_dialog(const char*path){
 if(!path||!dm_fs_writable(path)||strstr(path,"/desktop-mode/Trash/")){dm_status("Ripristina gli elementi del Cestino prima di rinominarli");return -1;}
 const char*name=strrchr(path,'/');if(!name||!name[1]||strlen(path)>=sizeof(rename_source))return -1;
 snprintf(rename_source,sizeof(rename_source),"%s",path);dm_prompt("Rinomina",name+1,renamed,NULL);return 0;
}
