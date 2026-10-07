#ifndef COMPAT_H
#define COMPAT_H
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#define RGBA8(r,g,b,a) ((unsigned)(r)|((unsigned)(g)<<8)|((unsigned)(b)<<16)|((unsigned)(a)<<24))
#define SCE_CTRL_MODE_ANALOG 1
#define SCE_TOUCH_PORT_FRONT 0
#define SCE_TOUCH_SAMPLING_STATE_START 1
#define SCE_CTRL_CROSS 1
#define SCE_CTRL_CIRCLE 2
#define SCE_CTRL_SQUARE 4
#define SCE_CTRL_START 8
#define SCE_CTRL_RTRIGGER 16
#define SCE_CTRL_LTRIGGER 32
#define SCE_O_RDONLY O_RDONLY
#define SCE_O_WRONLY O_WRONLY
#define SCE_O_CREAT O_CREAT
#define SCE_O_EXCL O_EXCL
#define SCE_O_TRUNC O_TRUNC
#define SCE_O_APPEND O_APPEND
#define SCE_SEEK_SET SEEK_SET
#define SCE_S_ISDIR S_ISDIR
typedef struct {unsigned buttons;unsigned char lx,ly;} SceCtrlData;
typedef struct {int x,y;} Report;
typedef struct {int reportNum;Report report[1];} SceTouchData;
typedef struct {unsigned st_mode;unsigned long long st_size,st_modified;} SceIoStat;
typedef struct {char d_name[256];SceIoStat d_stat;} SceIoDirent;
typedef struct Texture vita2d_texture;
typedef struct Font vita2d_pgf;
int vita2d_init(void);void vita2d_set_clear_color(unsigned);vita2d_pgf*vita2d_load_default_pgf(void);
void vita2d_draw_rectangle(float,float,float,float,unsigned);void vita2d_draw_line(float,float,float,float,unsigned);
int vita2d_pgf_text_width(vita2d_pgf*,float,const char*);
int vita2d_pgf_draw_text(vita2d_pgf*,int,int,unsigned,float,const char*);
vita2d_texture*vita2d_load_PNG_file(const char*);vita2d_texture*vita2d_load_JPEG_file(const char*);void vita2d_free_texture(vita2d_texture*);
void vita2d_draw_texture(vita2d_texture*,float,float);void vita2d_draw_texture_scale(vita2d_texture*,float,float,float,float);
void vita2d_draw_texture_part_scale(vita2d_texture*,float,float,float,float,float,float,float,float);
unsigned vita2d_texture_get_width(vita2d_texture*);unsigned vita2d_texture_get_height(vita2d_texture*);
void vita2d_start_drawing(void);void vita2d_clear_screen(void);void vita2d_end_drawing(void);void vita2d_swap_buffers(void);
int sceCtrlSetSamplingMode(int);int sceTouchSetSamplingState(int,int);int sceCtrlPeekBufferPositive(int,SceCtrlData*,int);int sceTouchPeek(int,SceTouchData*,int);int sceKernelDelayThread(unsigned);
int sceIoGetstat(const char*,SceIoStat*);int sceIoRmdir(const char*);int sceIoRename(const char*,const char*);
int sceIoWrite(int,const void*,unsigned);int sceIoRemove(const char*);
int sceIoDopen(const char*);int sceIoDclose(int);int sceIoDread(int,SceIoDirent*);int sceIoMkdir(const char*,int);int sceIoOpen(const char*,int,int);int sceIoRead(int,void*,unsigned);int sceIoClose(int);
long sceIoLseek(int,long,int);
#endif

vita2d_texture*vita2d_create_empty_texture(unsigned,unsigned);void*vita2d_texture_get_datap(vita2d_texture*);unsigned vita2d_texture_get_stride(vita2d_texture*);

#ifndef SCE_CTRL_SELECT
#define SCE_CTRL_SELECT 64
#endif
