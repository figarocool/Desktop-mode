#include "compat.h"
#include "../desktop_api.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
struct Texture {
    void *pixels;
    SDL_Texture*t;
    int w,h;
};
struct Font {
    TTF_Font*f;
};
static SDL_Window*window;
static SDL_Renderer*renderer;
static unsigned clear;
static unsigned buttons;
static int mx,my,down;
void desktop_pointer(int*x,int*y) {
    *x=mx;
    *y=my;
}
static char test_mount[512];
void desktop_test_mount(const char*p) {
    snprintf(test_mount,sizeof(test_mount),"%s",p);
}
static const char*map(const char*p) {
    static char out[2048];
    if(!strncmp(p,"app0:/",6))snprintf(out,sizeof(out),"native/%s",p+6);
    else if(!strncmp(p,"ux0:/",5))snprintf(out,sizeof(out),"native/desktop/demo/ux0/%s",p+5);
    else if(!strncmp(p,"ur0:/",5))snprintf(out,sizeof(out),"native/desktop/demo/ur0/%s",p+5);
    else if(!strncmp(p,"ud0:/",5)&&test_mount[0])snprintf(out,sizeof(out),"%.511s/%.1023s",test_mount,p+5);
    else if(strchr(p,':'))snprintf(out,sizeof(out),"native/desktop/demo/unmounted");
    else snprintf(out,sizeof(out),"%s",p);
    return out;
}
static void color(unsigned col) {
    SDL_SetRenderDrawColor(renderer,col&255,(col>>8)&255,(col>>16)&255,col>>24);
}
int vita2d_init(void) {
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)||TTF_Init()) {
        fprintf(stderr,"%s\n",SDL_GetError());
        exit(1);
    }
    IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
    SDL_StartTextInput();
    window=SDL_CreateWindow("Desktop Mode App — anteprima C / PS Vita",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,960,544,SDL_WINDOW_SHOWN);
    if(!window) {
        fprintf(stderr,"%s\n",SDL_GetError());
        exit(1);
    }
    renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!renderer)renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    return 0;
}
void vita2d_set_clear_color(unsigned c) {
    clear=c;
}
vita2d_pgf*vita2d_load_default_pgf(void) {
    vita2d_pgf*f=calloc(1,sizeof(*f));
    f->f=TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",18);
    if(!f->f)exit(1);
    return f;
}
void vita2d_draw_rectangle(float x,float y,float w,float h,unsigned col) {
    color(col);
    SDL_FRect r= {
        x,y,w,h
    };
    SDL_RenderFillRectF(renderer,&r);
}
void vita2d_draw_line(float x,float y,float a,float b,unsigned col) {
    color(col);
    SDL_RenderDrawLineF(renderer,x,y,a,b);
}
int vita2d_pgf_draw_text(vita2d_pgf*f,int x,int y,unsigned col,float scale,const char*s) {
    if(!s||!*s)return 0;
    SDL_Color sc= {
        col&255,(col>>8)&255,(col>>16)&255,col>>24
    };
    SDL_Surface*surface=TTF_RenderUTF8_Blended(f->f,s,sc);
    if(!surface)return 0;
    SDL_Texture*t=SDL_CreateTextureFromSurface(renderer,surface);
    SDL_FRect r= {
        x,y-TTF_FontAscent(f->f)*scale,surface->w*scale,surface->h*scale
    };
    SDL_RenderCopyF(renderer,t,0,&r);
    SDL_DestroyTexture(t);
    SDL_FreeSurface(surface);
    return 0;
}
vita2d_texture*vita2d_load_PNG_file(const char*p) {
    SDL_Surface*s=IMG_Load(map(p));
    if(!s)return 0;
    SDL_Surface*rgba=SDL_ConvertSurfaceFormat(s,SDL_PIXELFORMAT_ABGR8888,0);SDL_FreeSurface(s);if(!rgba)return 0;
    vita2d_texture*t=calloc(1,sizeof(*t));
    if(!t){SDL_FreeSurface(rgba);return 0;}
    t->w=rgba->w;t->h=rgba->h;t->pixels=malloc((size_t)t->w*t->h*4);
    t->t=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ABGR8888,SDL_TEXTUREACCESS_STREAMING,t->w,t->h);
    if(t->pixels)for(int y=0;y<t->h;y++)memcpy((unsigned char*)t->pixels+(size_t)y*t->w*4,(unsigned char*)rgba->pixels+(size_t)y*rgba->pitch,(size_t)t->w*4);
    SDL_FreeSurface(rgba);
    if(!t->pixels||!t->t){vita2d_free_texture(t);return 0;}
    SDL_SetTextureBlendMode(t->t,SDL_BLENDMODE_BLEND);SDL_UpdateTexture(t->t,NULL,t->pixels,t->w*4);
    return t;
}
vita2d_texture*vita2d_load_JPEG_file(const char*p) {
    return vita2d_load_PNG_file(p);
}
void vita2d_free_texture(vita2d_texture*t) {
    SDL_DestroyTexture(t->t);
    free(t->pixels);
    free(t);
}
void vita2d_draw_texture_scale(vita2d_texture*t,float x,float y,float sx,float sy) {
    if(t->pixels)SDL_UpdateTexture(t->t,NULL,t->pixels,t->w*4);
    SDL_FRect r= {
        x,y,t->w*sx,t->h*sy
    };
    SDL_RenderCopyF(renderer,t->t,0,&r);
}
void vita2d_draw_texture(vita2d_texture*t,float x,float y) {
    vita2d_draw_texture_scale(t,x,y,1,1);
}
unsigned vita2d_texture_get_width(vita2d_texture*t) {
    return t->w;
}
unsigned vita2d_texture_get_height(vita2d_texture*t) {
    return t->h;
}
void vita2d_start_drawing(void) {
}
void vita2d_clear_screen(void) {
    color(clear);
    SDL_RenderClear(renderer);
}
void vita2d_end_drawing(void) {
}
int desktop_capture_rgba(void*pixels,int pitch){return SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_RGBA32,pixels,pitch);}
void vita2d_swap_buffers(void) {
    if(getenv("DESKTOP_SCREENSHOT")) {
        SDL_Surface*s=SDL_CreateRGBSurfaceWithFormat(0,960,544,32,SDL_PIXELFORMAT_RGBA32);
        SDL_RenderReadPixels(renderer,0,SDL_PIXELFORMAT_RGBA32,s->pixels,s->pitch);
        IMG_SavePNG(s,getenv("DESKTOP_SCREENSHOT"));
        SDL_FreeSurface(s);
        exit(0);
    }
    SDL_RenderPresent(renderer);
}
int sceKernelDelayThread(unsigned n) {
    SDL_Delay(n/1000);
    return 0;
}
int sceCtrlSetSamplingMode(int a) {
    (void)a;
    return 0;
}
int sceTouchSetSamplingState(int a,int b) {
    (void)a;
    (void)b;
    return 0;
}
static struct {
    char text[1024];
    int key;
}
input_queue[128];
static unsigned input_read,input_write;
static void enqueue(const char*s,int key) {
    if(input_write-input_read>=128)return;
    unsigned idx=input_write++%128;
    snprintf(input_queue[idx].text,sizeof(input_queue[idx].text),"%s",s?s:"");
    input_queue[idx].key=key;
}
int desktop_input(char*out,size_t cap,int*key) {
    if(input_read==input_write)return 0;
    unsigned idx=input_read++%128;
    snprintf(out,cap,"%s",input_queue[idx].text);
    *key=input_queue[idx].key;
    return 1;
}
int desktop_import_dropped_file(const char *source,char *virtual_path,size_t path_cap) {
    static unsigned serial;
    if(!source||!*source)return -1;
    const char *base=strrchr(source,'/');
    const char *windows_base=strrchr(source,'\\');
    if(windows_base&&(!base||windows_base>base))base=windows_base;
    base=base?base+1:source;
    if(!*base||!strcmp(base,".")||!strcmp(base,".."))return -1;
    char directory[1200],destination[1400],file_name[256];FILE *in=fopen(source,"rb");if(!in)return -1;
    size_t base_length=strlen(base);if(base_length>=sizeof(file_name)){const char*dot=strrchr(base,'.');size_t ext_length=dot?strlen(dot):0;if(ext_length>40)ext_length=40;size_t stem=sizeof(file_name)-ext_length-1;memcpy(file_name,base,stem);if(dot)memcpy(file_name+stem,dot,ext_length);file_name[stem+ext_length]=0;}else snprintf(file_name,sizeof(file_name),"%s",base);
    char parent[1100];snprintf(parent,sizeof(parent),"native/desktop/demo/ux0/data/desktop-mode/external-imports");if(mkdir(parent,0777)<0&&errno!=EEXIST){fclose(in);return -1;}
    unsigned id=0;for(int tries=0;tries<10000;tries++){id=++serial;snprintf(directory,sizeof(directory),"%s/%u",parent,id);if(mkdir(directory,0777)==0)break;if(errno!=EEXIST){fclose(in);return -1;}directory[0]=0;}
    if(!directory[0]){fclose(in);return -1;}
    snprintf(destination,sizeof(destination),"%s/%s",directory,file_name);
    FILE *out=fopen(destination,"wb");if(!out){fclose(in);return -1;}
    unsigned char buffer[32768];size_t n;int failed=0;
    while((n=fread(buffer,1,sizeof(buffer),in))>0)if(fwrite(buffer,1,n,out)!=n){failed=1;break;}
    if(ferror(in))failed=1;
    if(fclose(in)!=0)failed=1;
    if(fclose(out)!=0)failed=1;
    if(failed){unlink(destination);return -1;}
    int wrote=snprintf(virtual_path,path_cap,"ux0:/data/desktop-mode/external-imports/%u/%s",id,file_name);
    return wrote>0&&(size_t)wrote<path_cap?0:-1;
}
static unsigned button(SDL_Keycode k) {
    switch(k) {
        case SDLK_ESCAPE:return SCE_CTRL_CIRCLE;
        case SDLK_F1:return SCE_CTRL_START;
        case SDLK_PAGEUP:return SCE_CTRL_LTRIGGER;
        case SDLK_PAGEDOWN:return SCE_CTRL_RTRIGGER;
        default:return 0;
    }
}
int sceCtrlPeekBufferPositive(int a,SceCtrlData*p,int n) {
    (void)a;
    (void)n;
    SDL_Event e;
    while(SDL_PollEvent(&e)) {
        if(e.type==SDL_QUIT)exit(0);
        if(e.type==SDL_DROPFILE){SDL_GetMouseState(&mx,&my);enqueue(e.drop.file,DM_KEY_FILEDROP);SDL_free(e.drop.file);}
        if(e.type==SDL_TEXTINPUT)enqueue(e.text.text,0);
        if(e.type==SDL_KEYDOWN) {
            buttons|=button(e.key.keysym.sym);
            int k=0;
            SDL_Keycode sym=e.key.keysym.sym;
            if(e.key.keysym.mod&KMOD_CTRL) {
                if(sym==SDLK_s)k=DM_KEY_SAVE;
                if(sym==SDLK_c)k=DM_KEY_COPY;
                if(sym==SDLK_v)k=DM_KEY_PASTE;
                if(sym==SDLK_x)k=DM_KEY_CUT;
                if(sym==SDLK_a)k=DM_KEY_SELECT_ALL;
            }      else {
                if(sym==SDLK_BACKSPACE)k=DM_KEY_BACKSPACE;
                if(sym==SDLK_RETURN)k=DM_KEY_ENTER;
                if(sym==SDLK_LEFT)k=DM_KEY_LEFT;
                if(sym==SDLK_RIGHT)k=DM_KEY_RIGHT;
                if(sym==SDLK_UP)k=DM_KEY_UP;
                if(sym==SDLK_DOWN)k=DM_KEY_DOWN;
                if(sym==SDLK_DELETE)k=DM_KEY_DELETE;
                if(sym==SDLK_HOME)k=DM_KEY_HOME;
                if(sym==SDLK_END)k=DM_KEY_END;
            }
            if(k&&(e.key.keysym.mod&KMOD_SHIFT)&&(k==DM_KEY_LEFT||k==DM_KEY_RIGHT||k==DM_KEY_HOME||k==DM_KEY_END||k==DM_KEY_UP||k==DM_KEY_DOWN))k|=DM_KEY_SHIFT;
            if(k)enqueue(NULL,k);
            if(button(sym))break;
        }
        if(e.type==SDL_KEYUP) {
            buttons&=~button(e.key.keysym.sym);
            if(button(e.key.keysym.sym))break;
        }
        if(e.type==SDL_MOUSEMOTION) {
            mx=e.motion.x;
            my=e.motion.y;
        }
        if(e.type==SDL_MOUSEWHEEL)enqueue(NULL,e.wheel.y>0?DM_KEY_SCROLL_UP:DM_KEY_SCROLL_DOWN);
        if(e.type==SDL_MOUSEBUTTONDOWN||e.type==SDL_MOUSEBUTTONUP) {
            mx=e.button.x;
            my=e.button.y;
            if(e.button.button==SDL_BUTTON_LEFT)down=e.type==SDL_MOUSEBUTTONDOWN;
            if(e.button.button==SDL_BUTTON_RIGHT) {
                if(e.type==SDL_MOUSEBUTTONDOWN)buttons|=SCE_CTRL_SQUARE;
                else buttons&=~SCE_CTRL_SQUARE;
            }
            break;
        }
    }
    p->buttons=buttons;
    p->lx=p->ly=128;
    return 1;
}
int sceTouchPeek(int a,SceTouchData*t,int n) {
    (void)a;
    (void)n;
    t->reportNum=down;
    t->report[0].x=mx*2;
    t->report[0].y=my*2;
    return 1;
}
static DIR*dirs[32];
static char roots[32][2048];
int sceIoDopen(const char*p) {
    const char*m=map(p);
    DIR*d=opendir(m);
    if(!d)return -1;
    for(int i=1;i<32;i++)if(!dirs[i]) {
        dirs[i]=d;
        snprintf(roots[i],2048,"%s",m);
        return i;
    }
    closedir(d);
    return -1;
}
int sceIoDclose(int fd) {
    closedir(dirs[fd]);
    dirs[fd]=0;
    return 0;
}
int sceIoDread(int fd,SceIoDirent*e) {
    struct dirent*d=readdir(dirs[fd]);
    if(!d)return 0;
    snprintf(e->d_name,256,"%s",d->d_name);
    char full[2304];
    snprintf(full,sizeof(full),"%s/%s",roots[fd],d->d_name);
    struct stat s;
    if(!stat(full,&s)) {
        e->d_stat.st_mode=s.st_mode;
        e->d_stat.st_size=s.st_size;
    }
    return 1;
}
int sceIoMkdir(const char*p,int mode) {
    return mkdir(map(p),mode);
}
int sceIoOpen(const char*p,int f,int mode) {
    return open(map(p),f,mode);
}
int sceIoRead(int fd,void*b,unsigned n) {
    return read(fd,b,n);
}
long sceIoLseek(int fd,long offset,int whence){return lseek(fd,offset,whence);}
int sceIoClose(int fd) {
    return close(fd);
}
int sceIoWrite(int fd,const void*b,unsigned n) {
    return write(fd,b,n);
}
int sceIoRemove(const char*p) {
    return unlink(map(p));
}
void vita2d_draw_texture_part_scale(vita2d_texture*t,float x,float y,float tx,float ty,float tw,float th,float sx,float sy) {
    if(t->pixels)SDL_UpdateTexture(t->t,NULL,t->pixels,t->w*4);
    SDL_Rect src= {
        (int)tx,(int)ty,(int)tw,(int)th
    };
    SDL_FRect dst= {
        x,y,tw*sx,th*sy
    };
    SDL_RenderCopyF(renderer,t->t,&src,&dst);
}
int vita2d_pgf_text_width(vita2d_pgf*f,float scale,const char*t) {
    int width=0,height=0;
    TTF_SizeUTF8(f->f,t,&width,&height);
    return (int)(width*scale);
}
int sceIoGetstat(const char*p,SceIoStat*out) {
    struct stat s;
    if(stat(map(p),&s)<0)return -1;
    out->st_mode=s.st_mode;
    out->st_size=s.st_size;
    out->st_modified=(uint64_t)s.st_mtim.tv_sec*1000000000ULL+s.st_mtim.tv_nsec;
    return 0;
}
int sceIoRmdir(const char*p) {
    return rmdir(map(p));
}
int sceIoRename(const char*a,const char*b) {
    char source[2048];
    snprintf(source,sizeof(source),"%s",map(a));
    return rename(source,map(b));
}

vita2d_texture*vita2d_create_empty_texture(unsigned w,unsigned h){
 vita2d_texture*t=calloc(1,sizeof(*t));if(!t)return NULL;t->w=w;t->h=h;t->pixels=calloc(w*h,4);t->t=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ABGR8888,SDL_TEXTUREACCESS_STREAMING,w,h);if(!t->pixels||!t->t){vita2d_free_texture(t);return NULL;}SDL_SetTextureBlendMode(t->t,SDL_BLENDMODE_BLEND);return t;
}
void*vita2d_texture_get_datap(vita2d_texture*t){return t->pixels;}
unsigned vita2d_texture_get_stride(vita2d_texture*t){return t->w*4;}

int desktop_selection_modifiers(void){SDL_Keymod m=SDL_GetModState();return(m&KMOD_CTRL?1:0)|(m&KMOD_SHIFT?2:0);}
