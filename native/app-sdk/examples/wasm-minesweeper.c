#include "../wasm_app.h"
#define DM_WIDGETS_WASM
#include "../dm_widgets.h"

#define SIDE 9
#define MINES 10
static unsigned char mine[SIDE][SIDE],shown[SIDE][SIDE],flagged[SIDE][SIDE];
static unsigned seed,elapsed;static int started,over,won,flag_mode,flags,view_width,view_height,grid_cell,grid_x,grid_y,button1_end,button2_start,button2_end;
static uint32_t rnd(void){seed=seed*1664525u+1013904223u;return seed;}
static int adjacent(int x,int y){int n=0;for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){int a=x+dx,b=y+dy;if(a>=0&&a<SIDE&&b>=0&&b<SIDE)n+=mine[b][a];}return n;}
static void reset(void){for(int y=0;y<SIDE;y++)for(int x=0;x<SIDE;x++)mine[y][x]=shown[y][x]=flagged[y][x]=0;seed=elapsed^0xC001D00Du;started=over=won=flags=0;}
static void place(int sx,int sy){int n=0;while(n<MINES){int x=rnd()%SIDE,y=rnd()%SIDE;if((x==sx&&y==sy)||mine[y][x])continue;mine[y][x]=1;n++;}started=1;}
static void reveal(int sx,int sy){int qx[81],qy[81],head=0,tail=0;shown[sy][sx]=1;qx[tail]=sx;qy[tail++]=sy;while(head<tail){int x=qx[head],y=qy[head++];if(adjacent(x,y))continue;for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)if(dx||dy){int a=x+dx,b=y+dy;if(a>=0&&a<SIDE&&b>=0&&b<SIDE&&!shown[b][a]&&!flagged[b][a]&&!mine[b][a]){shown[b][a]=1;qx[tail]=a;qy[tail++]=b;}}}}
static void check_win(void){int left=0;for(int y=0;y<SIDE;y++)for(int x=0;x<SIDE;x++)if(!mine[y][x]&&!shown[y][x])left++;if(!left){won=over=1;}}
uint32_t dm_app_abi_version(void){return DM_WASM_APP_ABI_VERSION;}
void dm_app_init(const char*a){(void)a;elapsed=0;reset();}
void dm_app_tick(uint32_t ms){elapsed+=ms;}
void dm_app_draw(int32_t w,int32_t h){
 view_width=w;view_height=h;const char*new_label="Nuova partita";const char*mode_label=flag_mode?"Modalita: bandiere":"Modalita: scopri";
 int x1=16,bw1=dm_host_text_width(new_label)+20,x2=x1+bw1+10,bw2=dm_host_text_width(mode_label)+20;
 if(bw1<116)bw1=116;if(bw2<120)bw2=120;button1_end=x1+bw1;button2_start=x2;button2_end=x2+bw2;
 dmw_button((DmWidgetRect){x1,4,bw1,31},new_label,0,0);
 dmw_button((DmWidgetRect){x2,4,bw2,31},mode_label,0,0);
 char info[72];int n=0;const char*p="Bandiere: ";for(int i=0;p[i];i++)info[n++]=p[i];unsigned f=flags;char d[12];int k=0;do{d[k++]='0'+f%10;f/=10;}while(f&&k<10);while(k)info[n++]=d[--k];info[n]=0;
 int info_x=x2+bw2+16,info_y=25;grid_y=48;
 if(info_x+dm_host_text_width(info)>w-12){info_x=16;info_y=59;grid_y=80;}
 dm_host_text(info_x,info_y,info,DM_WASM_COLOR_TEXT);
 if(over)dm_host_text(info_x+dm_host_text_width(info)+16,info_y,won?"Hai vinto!":"Partita terminata",won?0xFF187A35u:0xFFAA2222u);
 grid_cell=34;if(w/SIDE<grid_cell)grid_cell=w/SIDE;if((h-grid_y-8)/SIDE<grid_cell)grid_cell=(h-grid_y-8)/SIDE;if(grid_cell<12)grid_cell=12;
 grid_x=(w-SIDE*grid_cell)/2;for(int y=0;y<SIDE;y++)for(int x=0;x<SIDE;x++){int px=grid_x+x*grid_cell,py=grid_y+y*grid_cell;uint32_t c=shown[y][x]?0xFFD6DCE3u:0xFF5B7FA5u;dm_host_rect(px,py,grid_cell-2,grid_cell-2,c);if(flagged[y][x])dm_host_text(px+grid_cell/3,py+grid_cell*2/3,"F",0xFFB62020u);else if(shown[y][x]){if(mine[y][x])dm_host_text(px+grid_cell/3,py+grid_cell*2/3,"*",0xFFB62020u);else{int a=adjacent(x,y);if(a){char s[2]={(char)('0'+a),0};uint32_t color=a==1?0xFF1748AAu:a==2?0xFF177A35u:0xFFB02020u;dm_host_text(px+grid_cell/3,py+grid_cell*2/3,s,color);}}}}
}
void dm_app_click(int32_t x,int32_t y,uint32_t b){if(!b)return;if(y<40){if(dmw_button_click((DmWidgetRect){16,4,button1_end-16,31},x,y,0))reset();else if(dmw_button_click((DmWidgetRect){button2_start,4,button2_end-button2_start,31},x,y,0))flag_mode=!flag_mode;return;}int cx=(x-grid_x)/grid_cell,cy=(y-grid_y)/grid_cell;if(x<grid_x||cx<0||cx>=SIDE||cy<0||cy>=SIDE||y<grid_y||over)return;if(flag_mode){if(!shown[cy][cx]){flagged[cy][cx]=!flagged[cy][cx];flags+=flagged[cy][cx]?1:-1;}return;}if(flagged[cy][cx])return;if(!started)place(cx,cy);if(mine[cy][cx]){shown[cy][cx]=1;over=1;return;}reveal(cx,cy);check_win();}
void dm_app_key(int32_t key){if(key==DM_WASM_KEY_ENTER||key==DM_WASM_KEY_SAVE)reset();}
void dm_app_menu(int32_t command){if(command==DM_WASM_MENU_NEW||command==DM_WASM_MENU_RESET)reset();}
void dm_app_close(void){}
