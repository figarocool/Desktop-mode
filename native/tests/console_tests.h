static void console_command(DmWindow*w,const char*command){w->app->text(w,command);w->app->key(w,DM_KEY_ENTER);}
static void console_tests(const char*root,const char*host_root){
 DmWindow*w=dm_launch("console",root);assert(w&&w->used);
 char a[DM_PATH_MAX],b[DM_PATH_MAX],d[DM_PATH_MAX],command[2048],buffer[128];
 assert(dm_fs_join(d,sizeof(d),root,"console space")==0);
 console_command(w,"md \"console space\"");assert(dm_fs_is_directory(d));console_command(w,"cd \"console space\"");
 console_command(w,"touch \"one file.txt\"");assert(dm_fs_join(a,sizeof(a),d,"one file.txt")==0);assert(dm_fs_read(a,buffer,sizeof(buffer))==0);assert(dm_fs_write(a,"contents",8,0)==0);
 console_command(w,"copy \"one file.txt\" \"two file.txt\"");assert(dm_job_active());assert(!dm_close(w));wait_job();assert(dm_fs_join(b,sizeof(b),d,"two file.txt")==0);assert(dm_fs_read(b,buffer,sizeof(buffer))==8&&!strcmp(buffer,"contents"));
 console_command(w,"cp \"one file.txt\" \"two file.txt\"");wait_job();assert(dm_fs_read(b,buffer,sizeof(buffer))==8&&!strcmp(buffer,"contents"));
 console_command(w,"ren \"two file.txt\" renamed.txt");assert(dm_fs_read(b,buffer,sizeof(buffer))<0);assert(dm_fs_join(b,sizeof(b),d,"renamed.txt")==0);assert(dm_fs_read(b,buffer,sizeof(buffer))==8);
 console_command(w,"mv renamed.txt final.txt");assert(dm_fs_join(b,sizeof(b),d,"final.txt")==0);assert(dm_fs_read(b,buffer,sizeof(buffer))==8);
 console_command(w,"ls");console_command(w,"dir .");console_command(w,"cat \"one file.txt\"");console_command(w,"type final.txt");
 console_command(w,"delete final.txt");assert(dm_confirm_active());dm_confirm_cancel();assert(dm_fs_read(b,buffer,sizeof(buffer))==8);assert(dm_close(w));
 w=dm_launch("console",d);assert(w);console_command(w,"cd ..");console_command(w,"mkdir another");assert(dm_fs_join(a,sizeof(a),root,"another")==0&&dm_fs_is_directory(a));
 console_command(w,"mkdir \"unterminated");console_command(w,"no-such-command");console_command(w,"pwd");w->app->key(w,DM_KEY_UP);w->app->key(w,DM_KEY_COPY);assert(!strcmp(dm_clipboard_text_get(),"pwd"));w->app->key(w,DM_KEY_SELECT_ALL);w->app->text(w,"clear");w->app->key(w,DM_KEY_ENTER);
 console_command(w,"cd ux0:/../../");assert(!strcmp((char*)w->state,"ux0:/"));snprintf(command,sizeof(command),"cd \"%s\"",root);console_command(w,command);assert(!strcmp((char*)w->state,root));
 assert(dm_close(w));
 /* Confirmed delete uses a private ud0 volume, never the user's Trash. */
 char private_mount[2048];snprintf(private_mount,sizeof(private_mount),"%s/console-ud0",host_root);assert(mkdir(private_mount,0777)==0);desktop_test_mount(private_mount);
 assert(dm_fs_write("ud0:/remove.txt","private",7,1)==0);w=dm_launch("console","ud0:/");assert(w);console_command(w,"rm remove.txt");assert(dm_confirm_active());dm_confirm_click(600,330);assert(dm_job_active());wait_job();assert(dm_fs_read("ud0:/remove.txt",buffer,sizeof(buffer))<0);assert(dm_trash_has_items("ud0:"));assert(dm_close(w));desktop_test_mount("");
 printf("PASS: Console aliases, quotes, relative/absolute paths, root confinement, history, async copy/collision, rename, cancelled and confirmed deletion on isolated volume\n");
}
