static void utility_app_tests(const char*root){
 int calculator_row=-1;for(int i=0;i<app_count;i++){const DmApp*a=start_app(i);if(i)assert(strcasecmp(start_app(i-1)->title,a->title)<=0);if(!strcmp(a->id,"calculator"))calculator_row=i;}
 assert(calculator_row>=0&&calculator_row<START_ROWS);start=1;start_offset=0;px=50;py=174+calculator_row*30+12;click();
 DmWindow*c=top_window();assert(c&&c->used&&!strcmp(c->app->id,"calculator"));
 start=1;px=200;py=465;click();assert(start_offset==START_ROWS);px=50;click();assert(start_offset==0);start=0;
c->app->text(c,"12+3");c->app->key(c,DM_KEY_ENTER);c->app->key(c,DM_KEY_COPY);assert(!strcmp(dm_clipboard_text_get(),"15"));c->app->key(c,DM_KEY_ENTER);c->app->key(c,DM_KEY_COPY);assert(!strcmp(dm_clipboard_text_get(),"18"));c->app->text(c,"C9/0=");c->app->key(c,DM_KEY_COPY);assert(!strcmp(dm_clipboard_text_get(),"Errore"));c->app->text(c,"C1.5*2=");c->app->key(c,DM_KEY_COPY);assert(!strcmp(dm_clipboard_text_get(),"3"));
 DmWindow*t=dm_launch("taskmanager",NULL);assert(t&&t->used);t->app->tick(t,16);t->app->draw(t);DmTaskInfo tasks[DM_MAX_WINDOWS];int count=dm_tasks(tasks,DM_MAX_WINDOWS),row=-1;for(int i=0;i<count;i++)if(tasks[i].window==c)row=i;assert(row>=0);t->app->click(t,30,119+row*30);int task_width=(int)((t->w-24)/dm_ui_scale()),task_button=(task_width-60)/3;int show_x=20+task_button/2,minimize_x=20+(task_button+10)+task_button/2,close_x=20+2*(task_button+10)+task_button/2;t->app->click(t,minimize_x,t->h-50);assert(c->minimized);t->app->click(t,show_x,t->h-50);assert(!c->minimized);t->app->click(t,close_x,t->h-50);assert(!c->used);assert(dm_close(t));
 DmWindow*p=dm_launch("paint",NULL);assert(p&&p->used);p->app->draw(p);p->app->click(p,35,159);pointer_held=1;px=p->x+140;py=p->y+180;p->app->tick(p,16);pointer_held=0;p->app->tick(p,16);assert(!dm_close(p));assert(dm_confirm_active());dm_confirm_cancel();
 char target[DM_PATH_MAX],desktop_file[DM_PATH_MAX],filename[80];snprintf(filename,sizeof(filename),"paint-test-%ld.png",(long)getpid());
 assert(dm_fs_join(desktop_file,sizeof(desktop_file),DESK,filename)==0);
 p->app->key(p,DM_KEY_SAVE);assert(dm_file_dialog_active());dm_file_dialog_click(300,416);assert(prompt.active);prompt_text(filename);prompt_key(DM_KEY_ENTER);dm_file_dialog_click(740,460);assert(!dm_file_dialog_active());
 void*image=dm_image_load(desktop_file);assert(image);unsigned width,height;dm_image_size(image,&width,&height);assert(width==560&&height==240);dm_image_free(image);
 assert(dm_fs_join(target,sizeof(target),root,"drawing.png")==0);assert(dm_fs_copy(desktop_file,target)==0);p->app->click(p,60,179);p->app->key(p,DM_KEY_SAVE);assert(!dm_file_dialog_active());assert(sceIoRemove(desktop_file)==0);assert(dm_close(p));
 uint32_t pixels[4]={DM_COLOR(255,0,0,255),DM_COLOR(0,255,0,255),DM_COLOR(0,0,255,255),DM_COLOR(255,255,255,255)};
 assert(dm_image_save_png(target,2,2,pixels,1)<0);
 dm_open_file(target);DmWindow*v=top_window();assert(v&&!strcmp(v->app->id,"images"));v->app->draw(v);assert(dm_close(v));
 printf("PASS: separate calculator/taskmanager/paint/images modules, arithmetic/errors, task actions, PNG round trip, image associations, dirty Paint confirmation\n");
}
