#include "../app-sdk/desktop_plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
typedef struct{char display[64];double value,last;char operation;int fresh,error,repeat;} Calc;
static void open_calc(DmWindow*w,const char*a){(void)a;strcpy(((Calc*)w->state)->display,"0");}
static int apply(Calc*c,double b){double a=c->value;switch(c->operation){case '+':a+=b;break;case '-':a-=b;break;case '*':a*=b;break;case '/':if(b==0)return -1;a/=b;break;}if(!isfinite(a))return -1;c->value=a;snprintf(c->display,sizeof(c->display),"%.12g",a);return 0;}
static void input(Calc*c,char k){
 if(k=='C'){memset(c,0,sizeof(*c));strcpy(c->display,"0");return;}
 if(c->error)return;
 if((k>='0'&&k<='9')||k=='.'){if(c->fresh){strcpy(c->display,"0");c->fresh=0;}if(k=='.'&&strchr(c->display,'.'))return;size_t n=strlen(c->display);if(n>=16)return;if(!strcmp(c->display,"0")&&k!='.')n=0;c->display[n]=k;c->display[n+1]=0;c->repeat=0;return;}
 if(k=='B'){size_t n=strlen(c->display);if(n>1)c->display[n-1]=0;else strcpy(c->display,"0");return;}
 if(k=='S'){double d=-strtod(c->display,NULL);snprintf(c->display,sizeof(c->display),"%.12g",d);return;}
 if(k=='='){if(c->operation){double b=c->repeat?c->last:strtod(c->display,NULL);c->last=b;if(apply(c,b)<0)c->error=1;c->repeat=1;c->fresh=1;}}
 else if(k&&strchr("+-*/",k)){double b=strtod(c->display,NULL);if(c->operation&&!c->fresh){if(apply(c,b)<0)c->error=1;}else c->value=b;c->operation=k;c->fresh=1;c->repeat=0;}
 if(c->error)strcpy(c->display,"Errore");
}
static const char*keys[]={"7","8","9","/","C","4","5","6","*","<-","1","2","3","-","+/-","0",".","=","+",""};
static void draw(DmWindow*w){Calc*c=w->state;dm_rect(w->x+20,w->y+45,640,52,DM_COLOR(255,255,255,255));dm_text(w->x+35,w->y+80,c->display,DM_COLOR(24,38,55,255));for(int i=0;i<20;i++)if(keys[i][0]){int x=w->x+20+i%5*128,y=w->y+115+i/5*60;dm_rect(x,y,116,48,DM_COLOR(206,225,244,255));dm_text_center(x+58,y+31,keys[i],DM_COLOR(24,38,55,255));}}
static void click(DmWindow*w,int x,int y){if(x<20||x>=660||y<115||y>=355)return;int i=(y-115)/60*5+(x-20)/128;if((x-20)%128>=116||(y-115)%60>=48)return;input(w->state,i==9?'B':i==14?'S':keys[i][0]);}
static void text(DmWindow*w,const char*t){while(*t)input(w->state,*t++);}
static void key(DmWindow*w,int k){if(k==DM_KEY_COPY)dm_clipboard_text_set(((Calc*)w->state)->display);if(k==DM_KEY_ENTER)input(w->state,'=');if(k==DM_KEY_BACKSPACE)input(w->state,'B');if(k==DM_KEY_DELETE)input(w->state,'C');}
const DmApp dm_calculator_app={DM_API_VERSION,"calculator","Calcolatrice",sizeof(Calc),open_calc,draw,click,text,key,0,"",0};
DM_EXPORT_APP(dm_calculator_app)
