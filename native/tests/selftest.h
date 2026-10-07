#include <assert.h>
#include <unistd.h>
#include <dirent.h>
static void cleanup_test(const char*path) {
    DIR*d=opendir(path);
    if(!d) {
        unlink(path);
        return;
    }
    struct dirent*e;
    while((e=readdir(d))) {
        if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
        char p[2048];
        snprintf(p,sizeof(p),"%s/%s",path,e->d_name);
        cleanup_test(p);
    }
    closedir(d);
    rmdir(path);
}
static void wait_job(void) {
    for(int i=0;i<100000&&dm_job_active();i++)dm_job_tick();
    assert(!dm_job_active());
}
static void start_shortcut_tests(void){
 const DmApp*app=NULL;for(int i=0;i<app_count;i++)if(!strcmp(registry[i]->id,"notepad")){app=registry[i];break;}
 assert(app);
 char path[DM_PATH_MAX],contents[128];assert(dm_fs_join(path,sizeof(path),DESK,"Selftest Start Shortcut.dmlink")==0);sceIoRemove(path);
 snprintf(start_drag_id,sizeof(start_drag_id),"%s",app->id);snprintf(start_drag_title,sizeof(start_drag_title),"Selftest Start Shortcut");start_drag_moved=1;start=1;px=500;py=250;
 update_start_drag(0);
 assert(dm_fs_read(path,contents,sizeof(contents))>0&&!strcmp(contents,"app:notepad"));
 assert(draw_app_shortcut(path,0,0,56));sceIoRemove(path);refresh_desktop();
 puts("PASS: dragging a Start app creates and renders a valid desktop shortcut");
}
static void ui_scale_tests(void){int old=dm_preferences.font_percent;DmWindow test={.x=100,.y=80};DmWindow*prior=dm_current_window;dm_current_window=&test;const int values[]={70,85,110};for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++){dm_preferences.font_percent=values[i];int x=210,y=130,w=120,h=40,logical_x=x,logical_y=y;dm_ui_transform_rect(&x,&y,&w,&h);int px=x,py=y;dm_ui_untransform_window_pointer(&test,&px,&py);assert(abs(px-logical_x)<=1&&abs(py-logical_y)<=1);assert(abs(w-(int)(120*dm_ui_scale()+.5f))<=1&&abs(h-(int)(40*dm_ui_scale()+.5f))<=1);}dm_current_window=prior;dm_preferences.font_percent=old;puts("PASS: app font scale keeps drawing geometry and pointer coordinates aligned");}
static void shell_layout_tests(void){int count=order_count,old_px=px,old_py=py,active=prompt.active,font=dm_preferences.font_percent,size=dm_preferences.icon_size;order_count=1;assert(task_width()==190);order_count=8;int expected=(680-taskbar_tasks_x())/8;if(expected>190)expected=190;assert(task_width()==expected);dm_preferences.icon_size=56;dm_preferences.font_percent=85;int normal=icon_item_height(100,"paint-test-2.png");dm_preferences.font_percent=110;int large=icon_item_height(100,"paint-test-2.png");assert(normal>56+39&&large>normal);prompt.active=1;px=500;py=210;prompt_click();assert(prompt.active);px=50;py=300;prompt_click();assert(!prompt.active);order_count=count;px=old_px;py=old_py;prompt.active=active;dm_preferences.font_percent=font;dm_preferences.icon_size=size;puts("PASS: taskbar slots, dynamic icon bounds and outside click closes the complete keyboard prompt");}
#include "dialog_jobs.h"
#include "selection_apps.h"
#include "trash_drag.h"
#include "rename_tests.h"
#include "utility_apps.h"
#include "console_tests.h"
#include "pdf_paint_tests.h"
#include "multi_devices.h"
#include "file_types_saver.h"
#include "network_settings_tests.h"
static int selftest(void) {
    start_shortcut_tests();
    ui_scale_tests();
    shell_layout_tests();
    dm_prompt("Testo", "abé", NULL, NULL);
    prompt_key(DM_KEY_RIGHT); /* collapse the initial selection at the end */
    prompt_key(DM_KEY_LEFT);
    prompt_text("X");
    assert(!strcmp(prompt.text, "abXé"));
    prompt_key(DM_KEY_LEFT | DM_KEY_SHIFT);
    prompt_text("Q");
    assert(!strcmp(prompt.text, "abQé"));
    prompt_key(DM_KEY_HOME);
    prompt_key(DM_KEY_RIGHT);
    prompt_key(DM_KEY_RIGHT);
    prompt_key(DM_KEY_RIGHT);
    prompt_key(DM_KEY_DELETE);
    assert(!strcmp(prompt.text, "abQ"));
    prompt.active = 0;
    puts("PASS: shared text prompt caret, UTF-8 arrows, selection replacement and Delete");
    char root[DM_PATH_MAX],a[DM_PATH_MAX],b[DM_PATH_MAX],c[DM_PATH_MAX],buf[128];
    char host_root[]="native/desktop/demo/ux0/data/desktop-mode/test-XXXXXX";
    assert(mkdtemp(host_root));
    snprintf(root,sizeof(root),STORE "%s",strrchr(host_root,'/')+1);
    assert(dm_fs_join(a,sizeof(a),root,"hello.txt")==0);
    assert(dm_fs_write(a,"ciao\n",5,1)==0);
    assert(dm_fs_write(a,"overwrite",9,1)<0);
    assert(dm_fs_read(a,buf,sizeof(buf))==5&&!strcmp(buf,"ciao\n"));
    assert(dm_fs_join(b,sizeof(b),root,"copy.txt")==0);
    assert(dm_fs_copy(a,b)==0);
    assert(dm_fs_read(b,buf,sizeof(buf))==5&&!strcmp(buf,"ciao\n"));
    assert(dm_fs_copy(a,b)<0);
    assert(dm_fs_copy(a,a)<0);
    assert(dm_fs_join(c,sizeof(c),root,"../escape")<0);
    assert(!dm_fs_writable("vs0:/app/test"));
    assert(!dm_fs_writable("ux0:/data/../evil"));
    assert(!dm_fs_writable("ux0:/data/.."));
    DmWindow*e=dm_launch("explorer",root);
    assert(e&&((Explorer*)e->state)->count==2);
    Explorer*state=e->state;
    state->selected=0;
    context_window=e;
    run_action(ACT_COPY);
    assert(file_clipboard[0]);
    run_action(ACT_PASTE);
    wait_job();
    assert(((Explorer*)e->state)->count==3);
    snprintf(creation_dir,sizeof(creation_dir),"%s",root);
    creation_kind=ACT_FOLDER;
    created("folder",NULL);
    assert(dm_fs_join(c,sizeof(c),root,"folder")==0&&dm_fs_is_directory(c));
    char tree[DM_PATH_MAX],nested[DM_PATH_MAX];
    assert(dm_fs_join(tree,sizeof(tree),c,"inside.txt")==0);
    assert(dm_fs_write(tree,"nested",6,1)==0);
    assert(dm_fs_join(nested,sizeof(nested),c,"nested")==0);
    assert(dm_fs_copy(c,nested)<0);
    assert(dm_fs_join(nested,sizeof(nested),root,"folder-copy")==0);
    assert(dm_fs_copy(c,nested)==0);
    assert(dm_fs_join(tree,sizeof(tree),nested,"inside.txt")==0);
    assert(dm_fs_read(tree,buf,sizeof(buf))==6&&!strcmp(buf,"nested"));
    creation_kind=ACT_TEXT;
    created("new",NULL);
    assert(dm_fs_join(c,sizeof(c),root,"new.txt")==0&&dm_fs_read(c,buf,sizeof(buf))==0);
    DmWindow*n=dm_launch("notepad",a);
    assert(n);
    n->app->text(n,"mondo");
    n->app->key(n,DM_KEY_SAVE);
    assert(dm_fs_read(a,buf,sizeof(buf))==10&&!strcmp(buf,"ciao\nmondo"));
    n->app->key(n,DM_KEY_SELECT_ALL);
    n->app->key(n,DM_KEY_COPY);
    assert(!strcmp(dm_clipboard_text_get(),"ciao\nmondo"));
    n->app->key(n,DM_KEY_PASTE);
    assert(dm_close(n)==0&&dm_confirm_active());
    dm_confirm_cancel();
    n->app->key(n,DM_KEY_SAVE);
    assert(dm_close(n));
    assert(dm_register_app(registry[app_count-1])<0);
    assert(dm_launch("missing",NULL)==NULL);
    DmWindow*counter=dm_launch("counter",NULL);
    assert(counter);
    counter->app->click(counter,50,169);
    counter->app->draw(counter);
    assert(counter->used);
    assert(dm_close(counter));
    /* Mouse routing: root mount -> data folder, not an empty preview. */  DmWindow*computer=dm_launch("explorer","");
    Explorer*ce=computer->state;
    int ux=-1;
    for(int i=0;i<ce->count;i++)if(!strcmp(ce->entries[i].name,"ux0:"))ux=i;
    assert(ux>=0);
    px=computer->x+220;
    py=computer->y+110+ux*27;
    click();
    assert(!strcmp(ce->path,""));
    click();
    assert(!strcmp(ce->path,"ux0:/")&&ce->count>0);
    dm_maximize(computer);
    assert(computer->maximized&&computer->w==960&&computer->h==504);
    dm_maximize(computer);
    assert(!computer->maximized&&computer->w==700);
    IconPosition*pos=icon_position(0);
    int original_x=pos->x,original_y=pos->y;
    px=pos->x+30;
    py=pos->y+30;
    begin_icon(0);
    px+=80;
    py+=50;
    update_icon_drag(1);
    assert(pos->x==(original_x+80>854?854:original_x+80)&&pos->y==(original_y+50>408?408:original_y+50));
    update_icon_drag(0);
    assert(pending_icon==-1);
    pos->x=original_x;
    pos->y=original_y;
    save_positions();
    int old_apps=app_count,old_modules=dm_loaded_plugin_count();
    assert(dm_load_plugin("app0:/build/test-modules/fixture.dmapp")==0);
    assert(app_count==old_apps+1&&dm_loaded_plugin_count()==old_modules+1);
    assert(dm_load_plugin("app0:/build/test-modules/fixture.dmapp")==0&&app_count==old_apps+1);
    assert(dm_associate_extension("invalid","fixture")<0);
    assert(dm_associate_extension(".nope","missing")<0);
    assert(dm_fs_join(c,sizeof(c),root,"demo.fixture")==0);
    assert(dm_fs_write(c,"fixture",7,1)==0);
    dm_open_file(c);
    DmWindow*fixture=top_window();
    assert(fixture&&!strcmp(fixture->app->id,"fixture"));
    assert(!strcmp((char*)fixture->state,c));
    fixture->minimized=1;
    fixture->app->tick(fixture,16);
    assert(*(unsigned*)((char*)fixture->state+DM_PATH_MAX)==16);
    assert(dm_close(fixture));
    /* Context creation of a desktop shortcut and shortcut activation. */  snprintf(file_clipboard,sizeof(file_clipboard),"%s",root);
    snprintf(creation_dir,sizeof(creation_dir),"%s",DESK);
    creation_kind=ACT_SHORTCUT;
    char linkname[64];
    snprintf(linkname,sizeof(linkname),"test-%ld.dmlink",(long)getpid());
    created(linkname,NULL);
    assert(dm_fs_join(c,sizeof(c),DESK,linkname)==0);
    int before=order_count;
    open_path(c,0);
    assert(order_count==before+1);
    assert(!strcmp(((Explorer*)top_window()->state)->path,root));
    sceIoRemove(c);
    dialog_job_tests(root,host_root);
    while(order_count) {
        DmWindow*w=&windows[order[order_count-1]];
        assert(dm_close(w));
    }
    trash_drag_tests(root,host_root);
    selection_app_tests(root);
    rename_tests(root);
    utility_app_tests(root);
    console_tests(root,host_root);
    paint_viewport_tests();
    pdf_viewer_tests(root);
    multi_device_tests(root,host_root);
    extern void dm_network_test(const char*);
    dm_network_test(root);
    while(order_count) {
        DmWindow*w=top_window();
        assert(dm_close(w));
    }
    DmWindow*address_window=dm_launch("explorer",NULL);
    assert(address_window);
    Explorer*address_state=address_window->state;
    explorer_click(address_window,120,50);
    assert(address_state->address_focus);
    explorer_text(address_window,root);
    explorer_key(address_window,DM_KEY_ENTER);
    assert(!strcmp(address_state->path,root)&&!address_state->address_focus);
    explorer_click(address_window,120,50);
    explorer_text(address_window,"ux0:/missing-address-test-folder/");
    explorer_key(address_window,DM_KEY_ENTER);
    assert(!strcmp(address_state->path,root)&&address_state->address_focus&&address_state->status[0]);
    explorer_key(address_window,DM_KEY_SELECT_ALL);
    dm_clipboard_text_set(root);
    explorer_key(address_window,DM_KEY_PASTE);
    explorer_key(address_window,DM_KEY_SELECT_ALL);
    explorer_key(address_window,DM_KEY_COPY);
    assert(!strcmp(dm_clipboard_text_get(),root));
    explorer_key(address_window,DM_KEY_ENTER);
    explorer_click(address_window,120,50);
    explorer_text(address_window,"ux0:");
    explorer_key(address_window,DM_KEY_ENTER);
    assert(!strcmp(address_state->path,"ux0:/"));
    explorer_click(address_window,120,50);
    explorer_text(address_window,"Computer");
    explorer_key(address_window,DM_KEY_ENTER);
    assert(!address_state->path[0]);
    assert(dm_close(address_window));
    printf("PASS: Explorer editable address, Enter navigation, invalid path preserves folder, selection and clipboard, mount shorthand and Computer\n");
    while(order_count)assert(dm_close(top_window()));
    start=1;px=40;py=430;click();
    assert(prompt.active&&!start&&prompt.callback==run_command);
    prompt_done(0);assert(!order_count);
    run_command(root,NULL);assert(top_window()&&top_window()->app==&explorer_app&&!strcmp(((Explorer*)top_window()->state)->path,root));
    char run_file[DM_PATH_MAX];assert(!dm_fs_join(run_file,sizeof(run_file),root,"run file.txt"));assert(!dm_fs_write(run_file,"Esegui test",11,1));
    run_command("run file.txt",NULL);assert(top_window()&&!strcmp(top_window()->app->id,"notepad"));assert(dm_close(top_window()));
    char quoted[DM_PATH_MAX+3];snprintf(quoted,sizeof(quoted),"\"%s\"",run_file);run_command(quoted,NULL);assert(!strcmp(top_window()->app->id,"notepad"));assert(dm_close(top_window()));
    int run_count=order_count;run_command("ux0:/missing-run-test-file.txt",NULL);assert(order_count==run_count);
    run_command("../../README.md",NULL);assert(order_count==run_count);
    run_command("ux0:",NULL);assert(!strcmp(((Explorer*)top_window()->state)->path,"ux0:/"));assert(dm_close(top_window()));
    run_command("calculator",NULL);assert(!strcmp(top_window()->app->id,"calculator"));assert(dm_close(top_window()));
    run_command("app0:/build/test-modules/fixture.dmapp",NULL);assert(!strcmp(top_window()->app->id,"fixture"));assert(dm_close(top_window()));
    while(order_count)assert(dm_close(top_window()));
    assert(dm_rdp_start("short",3389)<0&&!dm_rdp_enabled());
    printf("PASS: Start Run entry, cancel, absolute/relative folders and associated files, quoted spaces, app IDs and modules, invalid paths, RDP password validation\n");
    file_types_saver_tests(root);
    network_settings_tests(host_root);
    cleanup_test(host_root);
    printf("PASS: files, copy files/folders, collision protection, traversal, new text/folder, shortcuts, Notepad save/clipboard/dirty close, app API, double-click folder navigation, icon drag, maximize/restore, runtime plugin load, extension dispatch, minimized app tick\n");
    return 0;
}
