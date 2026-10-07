static void network_settings_tests(const char*host_root){
 while(order_count)assert(dm_close(top_window()));
 assert(!dm_network_tray_click(669,520));assert(!dm_network_tray_click(700,520));assert(!dm_network_tray_click(685,500));
 assert(dm_network_tray_click(685,520));DmWindow*w=top_window();assert(w&&!strcmp(w->app->id,"connections"));w->app->draw(w);w->app->tick(w,2000);w->app->click(w,500,w->h-35);assert(dm_network_configure(0)<0);assert(dm_network_configure(1)<0);assert(dm_close(w));
 w=dm_launch("control",NULL);assert(w);w->app->click(w,380,375);assert(!strcmp(top_window()->app->id,"connections"));assert(dm_close(top_window()));assert(dm_close(w));
 /* All four application tray slots and the network icon have independent hit areas. */
 DmWindow*tray_windows[4];for(int i=0;i<4;i++){tray_windows[i]=dm_launch("about",NULL);assert(tray_windows[i]&&!dm_tray_set(tray_windows[i],"Test",1));}
 for(int i=0;i<4;i++){assert(dm_runtime_tray_click(555+i*30,520));assert(top_window()==tray_windows[i]);}
 assert(!dm_runtime_tray_click(685,520));assert(dm_network_tray_click(685,520));assert(!strcmp(top_window()->app->id,"connections"));assert(dm_close(top_window()));for(int i=0;i<4;i++)assert(dm_close(tray_windows[i]));
 mounts_initialized=0;poll_mounts(100);fs_dirty=0;DmWindow*e=dm_launch("explorer",NULL);assert(e);int before=((Explorer*)e->state)->count;
 desktop_test_mount(host_root);poll_mounts(2100);assert(fs_dirty);refresh_all();assert(((Explorer*)e->state)->count==before+1);
 int found=0;Explorer*s=e->state;for(int i=0;i<s->count;i++)if(!strcmp(s->entries[i].name,"ud0:"))found=1;assert(found);
 desktop_test_mount("");poll_mounts(4100);assert(fs_dirty);refresh_all();assert(((Explorer*)e->state)->count==before);assert(dm_close(e));
 mounts_initialized=mount_dialog_dirty=0;
 puts("PASS: network tray and Control Panel entry, live Linux adapter/refresh, unavailable configuration is explicit, four app tray slots preserved, mounted-volume insertion/removal updates Computer");
}
