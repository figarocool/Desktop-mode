#include "../wasm_app.h"
#define DM_WIDGETS_WASM
#include "../dm_widgets.h"

static char display[32]="0", operation;
static char input_buffer[64];
static double accumulator,last_operand;
static int fresh,error,repeating;
static int grid_x,grid_y,column_width,row_height;
static const char *labels[20]={"7","8","9","/","C","4","5","6","*","<-","1","2","3","-","+/-","0",".","=","+",""};

static double parse_display(void){
    unsigned i=0;int sign=1;double value=0.0,scale=0.1;
    if(display[i]=='-'){sign=-1;i++;}
    for(;display[i]&&display[i]!='.';i++)value=value*10.0+(display[i]-'0');
    if(display[i]=='.')for(i++;display[i];i++){value+=(display[i]-'0')*scale;scale*=0.1;}
    return sign*value;
}
static int format_number(double value,char out[32]){
    unsigned n=0;int negative=value<0.0;
    if(value!=value||value>999999999999.0||value< -999999999999.0)return 0;
    if(negative)value=-value;
    uint64_t whole=(uint64_t)value;
    uint32_t fraction=(uint32_t)((value-(double)whole)*1000000.0+0.5);
    if(fraction>=1000000){whole++;fraction-=1000000;}
    char digits[24];unsigned d=0;do{digits[d++]=(char)('0'+whole%10);whole/=10;}while(whole&&d<sizeof(digits));
    if(negative)out[n++]='-';while(d)out[n++]=digits[--d];
    if(fraction){out[n++]='.';uint32_t place=100000;for(int k=0;k<6;k++){out[n++]=(char)('0'+fraction/place%10);place/=10;}while(out[n-1]=='0')n--;if(out[n-1]=='.')n--;}
    out[n]=0;return 1;
}
static void format_value(double value){
    char formatted[32];if(!format_number(value,formatted)){error=1;display[0]=0;return;}
    unsigned i=0;do{display[i]=formatted[i];}while(formatted[i++]);
}
static void press(char key){
    if(key=='C'){display[0]='0';display[1]=0;accumulator=last_operand=0;operation=0;fresh=error=repeating=0;return;}
    if(error)return;
    if((key>='0'&&key<='9')||key=='.'){
        if(fresh){display[0]='0';display[1]=0;fresh=0;}
        if(key=='.'){for(unsigned i=0;display[i];i++)if(display[i]=='.')return;}
        unsigned n=0;while(display[n])n++;if(n>=20)return;
        if(display[0]=='0'&&display[1]==0&&key!='.')n=0;
        display[n]=(char)key;display[n+1]=0;repeating=0;return;
    }
    if(key=='B'){unsigned n=0;while(display[n])n++;if(n>1)display[n-1]=0;else{display[0]='0';display[1]=0;}return;}
    if(key=='S'){format_value(-parse_display());return;}
    if(key=='='){
        if(operation){double b=repeating?last_operand:parse_display();last_operand=b;double a=accumulator;
            if(operation=='+')a+=b;else if(operation=='-')a-=b;else if(operation=='*')a*=b;else if(operation=='/'){if(b==0.0){error=1;display[0]=0;return;}a/=b;}
            format_value(a);accumulator=a;repeating=1;fresh=1;
        }return;
    }
    if(key=='+'||key=='-'||key=='*'||key=='/'){
        double b=parse_display();if(operation&&!fresh){double a=accumulator;
            if(operation=='+')a+=b;else if(operation=='-')a-=b;else if(operation=='*')a*=b;else if(operation=='/'){if(b==0.0){error=1;display[0]=0;return;}a/=b;}
            accumulator=a;format_value(a);
        }else accumulator=b;
        operation=key;fresh=1;repeating=0;
    }
}
uint32_t dm_app_abi_version(void){return DM_WASM_APP_ABI_VERSION;}
void dm_app_init(const char *argument){(void)argument;display[0]='0';display[1]=0;accumulator=last_operand=0;operation=0;fresh=error=repeating=0;}
uint32_t dm_app_text_buffer(void){return (uint32_t)(uintptr_t)input_buffer;}
void dm_app_text(uint32_t n){if(n>sizeof(input_buffer)-1)n=sizeof(input_buffer)-1;input_buffer[n]=0;for(uint32_t i=0;i<n;i++)if(input_buffer[i]=='C'||input_buffer[i]=='c')press('C');else if((input_buffer[i]>='0'&&input_buffer[i]<='9')||input_buffer[i]=='.'||input_buffer[i]=='+'||input_buffer[i]=='-'||input_buffer[i]=='*'||input_buffer[i]=='/'||input_buffer[i]=='=')press(input_buffer[i]);}
void dm_app_key(int32_t key){if(key==DM_WASM_KEY_ENTER)press('=');else if(key==DM_WASM_KEY_COPY)dm_host_clipboard_set(error?"Errore":display);else if(key==DM_WASM_KEY_BACKSPACE)press('B');else if(key==DM_WASM_KEY_DELETE)press('C');}
void dm_app_draw(int32_t width,int32_t height){
    grid_x=20;int gap=8;column_width=(width-40-gap*4)/5;if(column_width<42)column_width=42;grid_y=110;row_height=(height-grid_y-12)/4;if(row_height>60)row_height=60;if(row_height<30)row_height=30;
    dm_host_rect(20,45,width-40,52,0xFFFFFFFFu);
    if(operation&&!error&&!repeating){char operand[32],expression[80];if(!format_number(accumulator,operand))operand[0]='0',operand[1]=0;unsigned n=0;for(unsigned i=0;operand[i]&&n+1<sizeof(expression);i++)expression[n++]=operand[i];if(n+2<sizeof(expression)){expression[n++]=' ';expression[n++]=operation;}if(!fresh&&n+1<sizeof(expression)){expression[n++]=' ';for(unsigned i=0;display[i]&&n+1<sizeof(expression);i++)expression[n++]=display[i];}expression[n]=0;dm_host_text(35,62,expression,0xFF68798Au);}
    dm_host_text(35,80,error?"Errore":display,DM_WASM_COLOR_TEXT);
    for(int i=0;i<20;i++)if(labels[i][0]){int col=i%5,row=i/5,x=grid_x+col*(column_width+gap),y=grid_y+row*row_height,bh=row_height-6;if(bh<24)bh=24;dmw_button((DmWidgetRect){x,y,column_width,bh},labels[i],0,0);}
}
void dm_app_click(int32_t x,int32_t y,uint32_t buttons){
    if(!buttons||x<grid_x||x>=grid_x+5*column_width+4*8||y<grid_y||y>=grid_y+4*row_height)return;
    int col=(x-grid_x)/(column_width+8),row=(y-grid_y)/row_height;
    if(col>=5||row>=4||(x-grid_x)%(column_width+8)>=column_width||(y-grid_y)%row_height>=row_height-6||!dmw_button_click((DmWidgetRect){grid_x+col*(column_width+8),grid_y+row*row_height,column_width,row_height-6},x,y,0))return;
    int i=row*5+col;
    if(i<0||i>=20)return;
    press(i==9?'B':i==14?'S':labels[i][0]);
}
void dm_app_menu(int32_t command){if(command==DM_WASM_MENU_COPY)dm_host_clipboard_set(display);else if(command==DM_WASM_MENU_RESET)press('C');}
void dm_app_close(void){}
