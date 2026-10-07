static void selection_app_tests(const char*root){
 char file[DM_PATH_MAX],buf[128];assert(dm_fs_join(file,sizeof(file),root,"selection.txt")==0);
 assert(dm_fs_write(file,"αβγ\nsecond",strlen("αβγ\nsecond"),1)==0);
 DmWindow*w=dm_launch("notepad",file);assert(w);w->app->draw(w);
 w->app->key(w,DM_KEY_HOME);w->app->key(w,DM_KEY_LEFT|DM_KEY_SHIFT);w->app->key(w,DM_KEY_LEFT|DM_KEY_SHIFT);
 w->app->key(w,DM_KEY_COPY);assert(!strcmp(dm_clipboard_text_get(),"γ\n"));
 w->app->key(w,DM_KEY_CUT);w->app->key(w,DM_KEY_SAVE);
 assert(dm_fs_read(file,buf,sizeof(buf))>=0&&!strcmp(buf,"αβsecond"));
 w->app->key(w,DM_KEY_PASTE);w->app->key(w,DM_KEY_SAVE);
 int pasted=dm_fs_read(file,buf,sizeof(buf));assert(pasted>=0&&!strcmp(buf,"αβγ\nsecond"));
 /* Shared pointer API: select a measured UTF-8 prefix by dragging. */
 w->app->click(w,18,84);pointer_held=1;px=w->x+34;py=w->y+84;
 w->app->tick(w,16);pointer_held=0;w->app->tick(w,16);w->app->key(w,DM_KEY_COPY);
 assert(!strcmp(dm_clipboard_text_get(),"αβ"));
 /* Joypad selection: anchor at start, Seleziona toggle, click endpoint. */
 w->app->click(w,18,84);w->app->key(w,DM_KEY_RIGHT|DM_KEY_SHIFT);w->app->key(w,DM_KEY_RIGHT|DM_KEY_SHIFT);w->app->key(w,DM_KEY_RIGHT|DM_KEY_SHIFT);
 w->app->key(w,DM_KEY_COPY);assert(!strcmp(dm_clipboard_text_get(),"αβγ"));
 w->app->key(w,DM_KEY_SELECT_ALL);w->app->text(w,"é");w->app->key(w,DM_KEY_SAVE);
 assert(dm_fs_read(file,buf,sizeof(buf))==2&&!strcmp(buf,"é"));
 w->app->key(w,DM_KEY_BACKSPACE);w->app->key(w,DM_KEY_SAVE);assert(dm_fs_read(file,buf,sizeof(buf))==0);
 assert(dm_close(w));
 /* Icon selection clears the old instruction without changing dragging. */
 dm_status("old message");begin_icon(0);assert(!status[0]);pending_icon=-1;
 /* Real registry/persistent removal, using private test settings only. */
 char settings[DM_PATH_MAX];assert(dm_fs_join(settings,sizeof(settings),root,"removed-test.txt")==0);
 dm_plugin_test_settings(settings);
 w=dm_launch("notepad",NULL);assert(w);assert(dm_plugin_uninstall("notepad")<0);assert(dm_close(w));
 assert(dm_plugin_uninstall("notepad")==0);assert(dm_launch("notepad",NULL)==NULL);
 assert(dm_fs_read(settings,buf,sizeof(buf))>0&&strstr(buf,"notepad.dmapp"));
 int before=dm_registered_count();dm_scan_plugins();assert(dm_registered_count()==before);assert(dm_launch("notepad",NULL)==NULL);
 assert(dm_plugin_reinstall("notepad")==0);w=dm_launch("notepad",NULL);assert(w);assert(dm_close(w));
 assert(dm_plugin_uninstall("browser")==0);assert(dm_launch("browser",NULL)==NULL);assert(dm_plugin_reinstall("browser")==0);
 dm_plugin_test_settings(NULL);
 printf("PASS: UTF-8 partial selection/cut/paste/replacement, pointer drag, joypad endpoints, removable Notepad/Browser, persistence, busy app protection, silent desktop selection\n");
}
