#include "../app-sdk/desktop_plugin.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdarg.h>
static int ascii_space(unsigned char ch){return ch==' '||ch=='\t'||ch=='\r'||ch=='\n';}
static int ascii_alnum(unsigned char ch){return(ch>='0'&&ch<='9')||(ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z');}
static char ascii_lower(unsigned char ch){return ch>='A'&&ch<='Z'?ch+('a'-'A'):ch;}
#define LINES 192
#define COMMAND 2048
#define INK DM_COLOR(210,230,210,255)
#define BG DM_COLOR(12,17,20,255)
typedef struct{
 char cwd[DM_PATH_MAX],command[COMMAND],log[LINES][256],history[16][COMMAND],pending[DM_PATH_MAX];
 int count,first,scroll,history_count,history_index,busy,select_all;size_t cursor;uint64_t revision;
} Console;
static void line(Console*c,const char*text){int slot;if(c->count==LINES){slot=c->first;c->first=(c->first+1)%LINES;}else slot=(c->first+c->count++)%LINES;snprintf(c->log[slot],256,"%.255s",text);c->scroll=0;}
static void emit(Console*c,const char*text){char row[256];size_t n=0;for(const unsigned char*p=(const unsigned char*)text;;p++){if(*p=='\n'||!*p||(n>=220&&(*p&0xc0)!=0x80)){row[n]=0;line(c,row);n=0;if(!*p)break;if(*p=='\n')continue;}if(*p>=32||*p=='\t')row[n++]=*p=='\t'?' ':*p;}}
static void output(Console*c,const char*format,...){char text[2048];va_list args;va_start(args,format);vsnprintf(text,sizeof(text),dm_ui_translate(format),args);va_end(args);emit(c,text);}
/* Canonical console paths stay in their console mount; .. never escapes it. */
static int resolve(Console*c,const char*input,char*out){
 if(!input||strlen(input)>=DM_PATH_MAX)return -1;
 char raw[DM_PATH_MAX],base[DM_PATH_MAX];snprintf(raw,sizeof(raw),"%s",input);for(char*p=raw;*p;p++)if(*p=='\\')*p='/';
 char*colon=strchr(raw,':'),*rest=raw;
 if(colon){size_t length=colon-raw;if(length<2||length>8)return -1;for(size_t i=0;i<length;i++)if(!ascii_alnum((unsigned char)raw[i]))return -1;if(colon[1]&&colon[1]!='/')return -1;snprintf(base,sizeof(base),"%.*s:/",(int)length,raw);rest=colon+1;}
 else if(raw[0]=='/'){const char*end=strchr(c->cwd,':');if(!end)return -1;snprintf(base,sizeof(base),"%.*s:/",(int)(end-c->cwd),c->cwd);}
 else snprintf(base,sizeof(base),"%s",c->cwd);
 if(strlen(base)>3&&base[strlen(base)-1]=='/')base[strlen(base)-1]=0;
 char*save=NULL,*part=strtok_r(rest,"/",&save);
 while(part){if(!strcmp(part,"..")){char*last=strrchr(base,'/');char*mount=strchr(base,':');if(last&&last>mount+1)*last=0;else if(mount)mount[1]=0;}
 else if(strcmp(part,".")){char next[DM_PATH_MAX];if(dm_fs_join(next,sizeof(next),base,part))return -1;snprintf(base,sizeof(base),"%s",next);}
 part=strtok_r(NULL,"/",&save);}
 if(base[strlen(base)-1]==':')strcat(base,"/");
 snprintf(out,DM_PATH_MAX,"%s",base);return 0;
}
static int tokenize(char*text,char**args,int capacity){
 char*read=text,*write=text;int count=0;
 while(*read){while(ascii_space((unsigned char)*read))read++;if(!*read)break;if(count==capacity)return -1;args[count++]=write;char quote=0;
 while(*read){char ch=*read++;if(quote){if(ch==quote)quote=0;else *write++=ch;}else if(ch=='"'||ch=='\'')quote=ch;else if(ascii_space((unsigned char)ch))break;else *write++=ch;}
 if(quote)return -1;
 *write++=0;}
 return count;
}
static void operation_done(int result,void*ctx){DmWindow*w=ctx;if(!w->used)return;Console*c=w->state;c->busy=0;output(c,result==1?"Operazione completata.":result==0?"Operazione annullata.":"Operazione fallita: verifica percorsi, spazio e permessi.");}
static void delete_confirmed(int yes,void*ctx){DmWindow*w=ctx;if(!w->used)return;Console*c=w->state;if(!yes){c->busy=0;output(c,"Eliminazione annullata.");return;}if(dm_trash_async(c->pending,operation_done,w)<0){c->busy=0;output(c,"Impossibile spostare nel Cestino: percorso protetto o operazione attiva.");}}
static void help(Console*c){output(c,"Console Desktop Mode - alias Windows / Linux");output(c,"dir / ls [cartella]     cd [cartella]     pwd / cwd");output(c,"copy / cp sorgente destinazione (anche cartelle)");output(c,"del / delete / rm / rmdir percorso -> Cestino, con conferma");output(c,"ren / rename / mv vecchio nuovo -> rinomina nella stessa cartella");output(c,"mkdir / md cartella     type / cat file     touch file");output(c,"echo testo    cls / clear    history    units / drives");output(c,"open percorso    start ID-app    ver / uname    exit");output(c,"Percorsi: ux0:/... o relativi; virgolette per nomi con spazi.");output(c,"Non esegue .exe, bash, script, pipe o opzioni GNU/CMD.");}
static void execute(DmWindow*w){
 Console*c=w->state;if(c->busy)return;dm_fs_path_update(c->cwd,sizeof(c->cwd),&c->revision);
 char command[COMMAND];snprintf(command,sizeof(command),"%s",c->command);c->command[0]=0;c->cursor=0;c->select_all=0;if(!command[0])return;
 output(c,"%s> %.900s",c->cwd,command);
 if(c->history_count==16){memmove(c->history,c->history+1,15*COMMAND);c->history_count--;}
 snprintf(c->history[c->history_count++],COMMAND,"%s",command);c->history_index=c->history_count;
 char*args[16];int n=tokenize(command,args,16);if(n<0){output(c,"Sintassi non valida: virgolette incomplete o troppi argomenti.");return;}if(!n)return;
 for(char*p=args[0];*p;p++)*p=ascii_lower((unsigned char)*p);
 const char*cmd=args[0];char source[DM_PATH_MAX],destination[DM_PATH_MAX];
 if(!strcmp(cmd,"help")||!strcmp(cmd,"?")){help(c);return;}
 if(!strcmp(cmd,"cls")||!strcmp(cmd,"clear")){c->count=c->first=c->scroll=0;return;}
 if(!strcmp(cmd,"pwd")||!strcmp(cmd,"cwd")){output(c,"%s",c->cwd);return;}
 if(!strcmp(cmd,"exit")){dm_close(w);return;}
 if(!strcmp(cmd,"history")){for(int i=0;i<c->history_count;i++)output(c,"%d  %s",i+1,c->history[i]);return;}
 if(!strcmp(cmd,"ver")||!strcmp(cmd,"uname")){DmSystemInfo info;dm_system_info(&info);output(c,"Desktop Mode API %d | %s | firmware %s",DM_API_VERSION,info.model,info.firmware);return;}
 if(!strcmp(cmd,"echo")){char text[COMMAND];size_t pos=0;for(int i=1;i<n;i++){if(i>1)text[pos++]=' ';for(const char*p=args[i];*p&&pos<sizeof(text)-1;p++)text[pos++]=*p;}text[pos]=0;output(c,"%s",text);return;}
 if(!strcmp(cmd,"units")||!strcmp(cmd,"drives")){const char*mounts[]={"ux0:/","uma0:/","imc0:/","ur0:/","ud0:/","vs0:/","os0:/","sa0:/","gro0:/","grw0:/"};for(unsigned i=0;i<sizeof(mounts)/sizeof(*mounts);i++)if(dm_fs_is_directory(mounts[i]))output(c,"%s",mounts[i]);return;}
 if(!strcmp(cmd,"start")){if(n!=2){output(c,"Uso: start ID-app (notepad, paint, calculator, images, console...)");return;}if(!dm_launch(args[1],NULL))output(c,"App non disponibile o limite di finestre raggiunto.");return;}
 if(!strcmp(cmd,"cd")||!strcmp(cmd,"chdir")){if(n==1){output(c,"%s",c->cwd);return;}if(n!=2||resolve(c,args[1],source)||!dm_fs_is_directory(source)){output(c,"Cartella non accessibile.");return;}snprintf(c->cwd,sizeof(c->cwd),"%s",source);return;}
 if(!strcmp(cmd,"dir")||!strcmp(cmd,"ls")){if(n>2||resolve(c,n==2?args[1]:".",source)){output(c,"Uso: dir/ls [cartella]");return;}DmDirectoryEntry entries[16];int offset=0,total=0;for(;;){int count=dm_fs_list(source,entries,16,offset);if(count<0){output(c,"Cartella non accessibile.");break;}for(int i=0;i<count;i++){if(entries[i].directory)output(c,"<DIR>            %s",entries[i].name);else output(c,"%12llu B   %s",(unsigned long long)entries[i].size,entries[i].name);}total+=count;offset+=count;if(count<16||total>=1024){output(c,"%d elementi%s",total,total>=1024?" (limite 1024; scorri il registro)":"");break;}}return;}
 if(n<2||resolve(c,args[1],source)){output(c,"Comando sconosciuto o percorso non valido. Usa help.");return;}
 if(!strcmp(cmd,"type")||!strcmp(cmd,"cat")){if(n!=2){output(c,"Uso: type/cat file");return;}char*text=malloc(DM_TEXT_MAX);if(!text){output(c,"Memoria insufficiente.");return;}int read=dm_fs_read(source,text,DM_TEXT_MAX);if(read<0)output(c,read==-2?"File oltre 32767 byte: usa un visualizzatore adatto.":"File non leggibile.");else{if(memchr(text,0,read))output(c,"File binario: usa l'app associata.");else emit(c,text);}free(text);return;}
 if(!strcmp(cmd,"open")){if(n!=2)output(c,"Uso: open percorso");else dm_open_file(source);return;}
 if(!strcmp(cmd,"mkdir")||!strcmp(cmd,"md")){if(n!=2||dm_fs_mkdir(source)<0)output(c,"Creazione cartella fallita: nome esistente o percorso non scrivibile.");else output(c,"Cartella creata.");return;}
 if(!strcmp(cmd,"touch")){if(n!=2){output(c,"Uso: touch file");return;}int result=dm_fs_write(source,"",0,1);output(c,result<0?"File esistente o percorso non scrivibile; contenuto conservato.":"File creato.");return;}
 if(!strcmp(cmd,"del")||!strcmp(cmd,"delete")||!strcmp(cmd,"rm")||!strcmp(cmd,"rmdir")||!strcmp(cmd,"rd")){if(n!=2){output(c,"Uso: del/rm percorso (nessun wildcard o flag)");return;}snprintf(c->pending,sizeof(c->pending),"%s",source);c->busy=1;if(dm_confirm("Console - Elimina","Spostare questo file o questa cartella nel Cestino?",delete_confirmed,w)<0){c->busy=0;output(c,"Conferma gia aperta.");}return;}
 if(!strcmp(cmd,"copy")||!strcmp(cmd,"cp")||!strcmp(cmd,"ren")||!strcmp(cmd,"rename")||!strcmp(cmd,"mv")){
 if(n!=3||resolve(c,args[2],destination)){output(c,"Uso: comando sorgente destinazione");return;}
 if(!strcmp(cmd,"copy")||!strcmp(cmd,"cp")){if(dm_fs_is_directory(destination)){char path[DM_PATH_MAX];const char*name=strrchr(source,'/');if(!name||dm_fs_join(path,sizeof(path),destination,name+1)){output(c,"Destinazione non valida.");return;}snprintf(destination,sizeof(destination),"%s",path);}c->busy=1;if(dm_copy_async(source,destination,operation_done,w)<0){c->busy=0;output(c,"Copia non avviata: percorso non valido o operazione attiva.");}}
 else {if(dm_fs_rename(source,destination)<0)output(c,"Rinomina fallita: stessa cartella richiesta, nome esistente o percorso protetto.");else output(c,"Elemento rinominato.");}return;}
 output(c,"Comando sconosciuto: %s. Usa help.",cmd);
}
static void open_console(DmWindow*w,const char*a){Console*c=w->state;snprintf(c->cwd,sizeof(c->cwd),"%s",a&&dm_fs_is_directory(a)?a:"ux0:/data/desktop-mode/Desktop/");dm_fs_path_update(c->cwd,sizeof(c->cwd),&c->revision);line(c,"Desktop Mode Console | comandi Windows + Linux | help");line(c,"Tastiera: clic sul comando | Su/Giu: cronologia | L/R: registro");}
static void insert(Console*c,const char*t){if(c->select_all){c->command[0]=0;c->cursor=0;c->select_all=0;}size_t length=strlen(c->command),add=strlen(t);if(length+add>=sizeof(c->command))return;memmove(c->command+c->cursor+add,c->command+c->cursor,length-c->cursor+1);memcpy(c->command+c->cursor,t,add);c->cursor+=add;}
static void text(DmWindow*w,const char*t){Console*c=w->state;if(!c->busy)insert(c,t);}
static void entered(const char*t,void*ctx){DmWindow*w=ctx;if(w->used){Console*c=w->state;snprintf(c->command,sizeof(c->command),"%s",t);c->cursor=strlen(c->command);execute(w);}}
static void key(DmWindow*w,int key){Console*c=w->state;if(c->busy)return;
 if(key==DM_KEY_ENTER){execute(w);return;}if(key==DM_KEY_SELECT_ALL){c->select_all=1;return;}if(key==DM_KEY_COPY){dm_clipboard_text_set(c->command);return;}if(key==DM_KEY_PASTE){insert(c,dm_clipboard_text_get());return;}
 if(key==DM_KEY_SCROLL_UP){c->scroll+=8;if(c->scroll>c->count)c->scroll=c->count;return;}if(key==DM_KEY_SCROLL_DOWN){c->scroll-=8;if(c->scroll<0)c->scroll=0;return;}
 if(key==DM_KEY_UP||key==DM_KEY_DOWN){if(key==DM_KEY_UP&&c->history_index>0)c->history_index--;if(key==DM_KEY_DOWN&&c->history_index<c->history_count)c->history_index++;snprintf(c->command,sizeof(c->command),"%s",c->history_index<c->history_count?c->history[c->history_index]:"");c->cursor=strlen(c->command);c->select_all=0;return;}
 if(key==DM_KEY_BACKSPACE&&c->select_all){c->command[0]=0;c->cursor=0;c->select_all=0;return;}c->select_all=0;
 if(key==DM_KEY_HOME)c->cursor=0;
 if(key==DM_KEY_END)c->cursor=strlen(c->command);
 if(key==DM_KEY_LEFT||key==DM_KEY_BACKSPACE){if(c->cursor){size_t pos=c->cursor-1;while(pos&&(c->command[pos]&0xc0)==0x80)pos--;if(key==DM_KEY_BACKSPACE)memmove(c->command+pos,c->command+c->cursor,strlen(c->command+c->cursor)+1);c->cursor=pos;}}
 if(key==DM_KEY_RIGHT&&c->command[c->cursor]){c->cursor++;while((c->command[c->cursor]&0xc0)==0x80)c->cursor++;}
}
static void click(DmWindow*w,int x,int y){Console*c=w->state;if(c->busy)return;if(y>=w->h-65&&y<w->h-30){if(x>=w->w-150&&x<w->w-20)execute(w);else if(x>=20&&x<w->w-160)dm_prompt("Console - comando",c->command,entered,w);}if(y>=w->h-29&&x>=20&&x<135)key(w,DM_KEY_SCROLL_UP);else if(y>=w->h-29&&x>=145&&x<260)key(w,DM_KEY_SCROLL_DOWN);}
static void draw(DmWindow*w){Console*c=w->state;dm_rect(w->x+5,w->y+32,w->w-10,w->h-37,BG);char cwd[100];snprintf(cwd,sizeof(cwd),"%.80s>",c->cwd);dm_text_raw(w->x+20,w->y+56,cwd,DM_COLOR(100,220,160,255));int rows=(w->h-145)/20,end=c->count-c->scroll;if(end<0)end=0;int first=end-rows;if(first<0)first=0;for(int i=first;i<end;i++){char row[256];snprintf(row,sizeof(row),"%s",c->log[(c->first+i)%LINES]);while(strlen(row)&&dm_text_width_raw(row)>w->w-45)row[strlen(row)-1]=0;dm_text_raw(w->x+20,w->y+82+(i-first)*20,row,INK);}dm_rect(w->x+20,w->y+w->h-65,w->w-185,35,c->select_all?DM_COLOR(45,80,125,255):DM_COLOR(35,42,48,255));char command[COMMAND];snprintf(command,sizeof(command),"%s",c->command);while(strlen(command)&&dm_text_width_raw(command)>w->w-205)command[strlen(command)-1]=0;dm_text_raw(w->x+27,w->y+w->h-41,command,INK);dm_rect(w->x+w->w-150,w->y+w->h-65,130,35,DM_COLOR(37,88,117,255));dm_text(w->x+w->w-130,w->y+w->h-41,c->busy?"In corso":"Esegui",INK);dm_text(w->x+20,w->y+w->h-10,"< Registro",INK);dm_text(w->x+145,w->y+w->h-10,"Registro >",INK);if(!c->busy&&!c->select_all){char prefix[COMMAND];size_t n=c->cursor;if(n>=sizeof(prefix))n=sizeof(prefix)-1;memcpy(prefix,c->command,n);prefix[n]=0;int width=dm_text_width_raw(prefix);if(width<w->w-205)dm_rect(w->x+27+width,w->y+w->h-59,1,23,INK);}}
static int close_console(DmWindow*w){Console*c=w->state;if(c->busy){dm_status("Attendi o annulla l'operazione prima di chiudere Console");return 0;}return 1;}
static void tick(DmWindow*w,unsigned ms){(void)ms;Console*c=w->state;dm_fs_path_update(c->cwd,sizeof(c->cwd),&c->revision);}
const DmApp dm_console_app={DM_API_VERSION,"console","Console",sizeof(Console),open_console,draw,click,text,key,close_console,"",tick};
DM_EXPORT_APP(dm_console_app)
