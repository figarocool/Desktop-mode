static void unused_file_result(const char*p,int replace,void*ctx){(void)p;(void)replace;(void)ctx;}
static void rename_tests(const char*root){
 char old[DM_PATH_MAX],name[DM_PATH_MAX],child[DM_PATH_MAX],updated[DM_PATH_MAX],buf[128];
 assert(dm_fs_join(old,sizeof(old),root,"rename-dir")==0);assert(sceIoMkdir(old,0777)==0);
 assert(dm_fs_join(child,sizeof(child),old,"note.txt")==0);assert(dm_fs_write(child,"original",8,1)==0);
 DmWindow*expl=dm_launch("explorer",old);assert(expl);
 DmWindow*note=dm_launch("notepad",child);assert(note);
 snprintf(file_clipboard,sizeof(file_clipboard),"%s",child);
 snprintf(updated,sizeof(updated),"%s",child);uint64_t seen=0;dm_fs_path_update(updated,sizeof(updated),&seen);
 assert(dm_fs_join(name,sizeof(name),root,"renamed-dir")==0);assert(dm_fs_rename(old,name)==0);
 assert(!dm_fs_is_directory(old));assert(!strcmp(((Explorer*)expl->state)->path,name));
 assert(dm_fs_join(child,sizeof(child),name,"note.txt")==0);assert(!strcmp(file_clipboard,child));
 dm_fs_path_update(updated,sizeof(updated),&seen);assert(!strcmp(updated,child));
 note->app->key(note,DM_KEY_SELECT_ALL);note->app->text(note,"changed");note->app->key(note,DM_KEY_SAVE);
 int written=dm_fs_read(child,buf,sizeof(buf));assert(written==7&&!strcmp(buf,"changed"));
 assert(dm_close(note));assert(dm_close(expl));
 assert(dm_fs_join(old,sizeof(old),name,"collision.txt")==0);assert(dm_fs_write(old,"keep",4,1)==0);
 assert(dm_fs_rename(child,old)<0);assert(dm_fs_read(old,buf,sizeof(buf))==4&&!strcmp(buf,"keep"));
 assert(dm_fs_rename(child,"vs0:/blocked.txt")<0);assert(dm_fs_rename("ux0:/data/desktop-mode",name)<0);
 assert(dm_rename_dialog(child)==0);prompt_done(0);assert(dm_fs_read(child,buf,sizeof(buf))==7);
 assert(dm_rename_dialog(child)==0);prompt_text("new.txt");assert(!strcmp(prompt.text,"new.txt"));prompt_key(DM_KEY_ENTER);
 assert(!prompt.active);assert(dm_fs_join(child,sizeof(child),name,"new.txt")==0);assert(dm_fs_read(child,buf,sizeof(buf))==7);
 DmFileDialogOptions o={DM_FILE_OPEN,"Rename test",name,NULL,NULL};assert(dm_file_dialog(&o,unused_file_result,NULL)==0);
 dm_file_dialog_click(145,164); /* Second sorted row: new.txt. */
 dm_file_dialog_click(560,105);assert(prompt.active);prompt_text("picker.txt");prompt_key(DM_KEY_ENTER);
 assert(dm_fs_join(child,sizeof(child),name,"picker.txt")==0);assert(dm_fs_read(child,buf,sizeof(buf))==7);dm_file_dialog_cancel();
 printf("PASS: rename file/folder, collision protection, cancel, prompt Enter, picker rename, Explorer/clipboard/Notepad path tracking\n");
}
