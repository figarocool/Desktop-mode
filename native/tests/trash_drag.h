static void trash_drag_tests(const char*root,const char*host_root){
 IconPosition*trash=icon_position(4);int tx=trash->x,ty=trash->y;trash->x=20;trash->y=300;
 /* Desktop drop asks before doing anything, and cancellation restores position. */
 char name[128],desktop_file[DM_PATH_MAX];snprintf(name,sizeof(name),"drag-%s.txt",strrchr(root,'/')+1);
 assert(dm_fs_join(desktop_file,sizeof(desktop_file),DESK,name)==0);assert(dm_fs_write(desktop_file,"keep",4,1)==0);
 refresh_desktop();int index=-1;for(int i=0;i<desk_count;i++)if(!strcmp(desk_entries[i].name,name))index=i+8;assert(index>=8);
 IconPosition*source=icon_position(index);int sx=source->x,sy=source->y;
 px=sx+35;py=sy+20;begin_icon(index);px=trash->x+50;py=trash->y+25;update_icon_drag(1);assert(over_trash());update_icon_drag(0);
 assert(dm_confirm_active()&&!strcmp(delete_target,desktop_file)&&source->x==sx&&source->y==sy);dm_confirm_cancel();
 SceIoStat stat;assert(sceIoGetstat(desktop_file,&stat)>=0);sceIoRemove(desktop_file);refresh_desktop();
 /* Explorer drag uses a private fake unit; no user trash is emptied. */
 char host_mount[1200];snprintf(host_mount,sizeof(host_mount),"%s/drag-unit",host_root);assert(mkdir(host_mount,0777)==0);desktop_test_mount(host_mount);
 assert(sceIoMkdir("ud0:/data",0777)>=0);assert(!dm_trash_has_items("ud0:"));assert(dm_fs_write("ud0:/data/drag.txt","drag file",9,1)==0);
 DmWindow*w=dm_launch("explorer","ud0:/data/");assert(w);Explorer*e=w->state;assert(e->count==1);
 px=w->x+180;py=w->y+110;w->app->click(w,180,110);assert(file_drag_window==w);
 px=trash->x+45;py=trash->y+20;update_file_drag(1);assert(file_drag_moved&&over_trash());update_file_drag(0);
 assert(dm_confirm_active()&&!strcmp(delete_target,"ud0:/data/drag.txt"));dm_confirm_cancel();assert(sceIoGetstat("ud0:/data/drag.txt",&stat)>=0);
 e->click_valid=0;px=w->x+180;py=w->y+110;w->app->click(w,180,110);px=trash->x+45;py=trash->y+20;update_file_drag(1);update_file_drag(0);
 assert(dm_confirm_active());dm_confirm_click(600,330);wait_job();assert(sceIoGetstat("ud0:/data/drag.txt",&stat)<0);
 char files[DM_PATH_MAX],info[DM_PATH_MAX];assert(dm_trash_directories("ud0:",files,info)==0);assert(dm_fs_is_directory(files));assert(dm_trash_has_items("ud0:"));
 assert(dm_job_empty_trash_unit("ud0:",NULL,NULL)==0);wait_job();assert(!dm_trash_has_items("ud0:"));
 assert(dm_close(w));desktop_test_mount("");trash->x=tx;trash->y=ty;
 puts("PASS: desktop and Explorer drag to trash, target highlight, confirmation/cancel, source position restoration, confirmed move on isolated unit");
}
