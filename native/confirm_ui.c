#include "desktop_api.h"
#include <stdio.h>
#include <string.h>
static struct {
    int active;
    char title[120],message[512];
    DmConfirmResult callback;
    void*context;
}
confirm;
int dm_confirm(const char*title,const char*message,DmConfirmResult cb,void*ctx) {
    if(confirm.active||!cb)return -1;
    confirm.active=1;
    snprintf(confirm.title,sizeof(confirm.title),"%s",title);
    snprintf(confirm.message,sizeof(confirm.message),"%s",message);
    confirm.callback=cb;
    confirm.context=ctx;
    return 0;
}
int dm_confirm_active(void) {
    return confirm.active;
}
static void finish(int yes) {
    DmConfirmResult cb=confirm.callback;
    void*ctx=confirm.context;
    memset(&confirm,0,sizeof(confirm));
    if(cb)cb(yes,ctx);
}
void dm_confirm_cancel(void) {
    if(confirm.active)finish(0);
}
void dm_confirm_click(int x,int y) {
    if(!confirm.active)return;
    if(y>=316&&y<356) {
        if(x>=520&&x<665)finish(1);
        else if(x>=290&&x<455)finish(0);
    }
}
void dm_confirm_draw(void) {
    if(!confirm.active)return;
    dm_rect(0,0,960,544,DM_COLOR(0,8,22,165));
    dm_rect(245,180,470,192,DM_COLOR(238,246,254,255));
    dm_rect(245,180,470,30,DM_COLOR(37,72,133,255));
    dm_text(260,202,confirm.title,DM_COLOR(255,255,255,255));
    char line[60];
    size_t len=strlen(confirm.message),pos=0;
    for(int row=0;row<4&&pos<len;row++) {
        size_t n=len-pos>53?53:len-pos;
        while(n&&((unsigned char)confirm.message[pos+n]&0xc0)==0x80)n--;
        memcpy(line,confirm.message+pos,n);
        line[n]=0;
        dm_text(260,238+row*20,line,DM_COLOR(24,38,55,255));
        pos+=n;
    }
    dm_rect(290,316,165,40,DM_COLOR(207,224,243,255));
    dm_text_center(372,342,"Annulla",DM_COLOR(24,38,55,255));
    dm_rect(520,316,145,40,DM_COLOR(56,108,173,255));
    dm_text_center(592,342,"Conferma",DM_COLOR(255,255,255,255));
}
