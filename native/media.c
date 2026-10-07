#include "desktop_api.h"
#include "app_runtime.h"
#include <vita2d.h>
#include <png.h>
#include <webp/decode.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
static void *load_webp(const char *path){
 SceIoStat stat;int fd=-1;uint8_t *encoded=NULL,*rgba=NULL;void *texture=NULL;int width=0,height=0;
 if(!path||sceIoGetstat(path,&stat)<0||stat.st_size<12||stat.st_size>32*1024*1024||SCE_S_ISDIR(stat.st_mode))return NULL;
 fd=sceIoOpen(path,SCE_O_RDONLY,0);if(fd<0)return NULL;
 size_t size=(size_t)stat.st_size,read_bytes=0;encoded=malloc(size);if(!encoded)goto done;
 while(read_bytes<size){int n=sceIoRead(fd,encoded+read_bytes,size-read_bytes);if(n<=0)goto done;read_bytes+=(size_t)n;}
 if(memcmp(encoded,"RIFF",4)||memcmp(encoded+8,"WEBP",4)||!WebPGetInfo(encoded,size,&width,&height))goto done;
 if(width<=0||height<=0||width>4096||height>4096||(uint64_t)width*(uint64_t)height>16u*1024u*1024u)goto done;
 rgba=WebPDecodeRGBA(encoded,size,&width,&height);if(!rgba)goto done;
 texture=vita2d_create_empty_texture((unsigned)width,(unsigned)height);if(!texture)goto done;
 {unsigned char *pixels=vita2d_texture_get_datap(texture);unsigned stride=vita2d_texture_get_stride(texture);for(int y=0;y<height;y++)memcpy(pixels+(size_t)y*stride,rgba+(size_t)y*(size_t)width*4,(size_t)width*4);}
done:
 if(fd>=0)sceIoClose(fd);
 free(encoded);
 if(rgba)WebPFree(rgba);
 return texture;
}
void*dm_image_load(const char*path){if(!path)return NULL;const char*e=strrchr(path,'.');if(!e)return NULL;void*t=NULL;if(!dm_ascii_casecmp(e,".png"))t=vita2d_load_PNG_file(path);if(!dm_ascii_casecmp(e,".jpg")||!dm_ascii_casecmp(e,".jpeg"))t=vita2d_load_JPEG_file(path);if(!dm_ascii_casecmp(e,".webp"))t=load_webp(path);if(t)dm_memory_image(t,(uint64_t)vita2d_texture_get_stride(t)*vita2d_texture_get_height(t));return t;}
void dm_image_update(void*image,const uint32_t*pixels){if(!image||!pixels)return;unsigned w=vita2d_texture_get_width(image),h=vita2d_texture_get_height(image),stride=vita2d_texture_get_stride(image);unsigned char*p=vita2d_texture_get_datap(image);for(unsigned y=0;y<h;y++)memcpy(p+y*stride,pixels+y*w,w*4);}
int dm_image_read(void*image,uint32_t*pixels,unsigned capacity){if(!image||!pixels)return -1;unsigned w=vita2d_texture_get_width(image),h=vita2d_texture_get_height(image),stride=vita2d_texture_get_stride(image);uint64_t bytes=(uint64_t)w*h*4;if(bytes>capacity)return -1;unsigned char*source=vita2d_texture_get_datap(image);if(!source)return -1;for(unsigned y=0;y<h;y++)memcpy(pixels+y*w,source+y*stride,w*4);return (int)bytes;}
void*dm_image_create(unsigned w,unsigned h,const uint32_t*pixels){if(!w||!h||w>4096||h>4096)return NULL;void*t=vita2d_create_empty_texture(w,h);if(t){dm_image_update(t,pixels);dm_memory_image(t,(uint64_t)vita2d_texture_get_stride(t)*h);}return t;}
void dm_image_draw_clipped(void*t,int x,int y,int w,int h,int cx,int cy,int cw,int ch){if(!t||w<=0||h<=0||cw<=0||ch<=0)return;dm_ui_transform_rect(&x,&y,&w,&h);dm_ui_transform_rect(&cx,&cy,&cw,&ch);unsigned tw=vita2d_texture_get_width(t),th=vita2d_texture_get_height(t);int ox=x,oy=y,ow=w,oh=h,x2=x+w,y2=y+h,cx2=cx+cw,cy2=cy+ch;if(x<cx)x=cx;if(y<cy)y=cy;if(x2>cx2)x2=cx2;if(y2>cy2)y2=cy2;if(dm_current_window){int l=dm_current_window->x,top=dm_current_window->y,r=l+dm_current_window->w,b=top+dm_current_window->h;if(x<l)x=l;if(y<top)y=top;if(x2>r)x2=r;if(y2>b)y2=b;}if(x2<=x||y2<=y)return;unsigned sx=(unsigned)((uint64_t)(x-ox)*tw/(unsigned)ow),sy=(unsigned)((uint64_t)(y-oy)*th/(unsigned)oh),sw=(unsigned)((uint64_t)(x2-x)*tw/(unsigned)ow),sh=(unsigned)((uint64_t)(y2-y)*th/(unsigned)oh);if(sw&&sh)vita2d_draw_texture_part_scale(t,x,y,sx,sy,sw,sh,(float)(x2-x)/sw,(float)(y2-y)/sh);}
void dm_image_draw(void*t,int x,int y,int w,int h){if(!t||w<=0||h<=0)return;dm_ui_transform_rect(&x,&y,&w,&h);unsigned tw=vita2d_texture_get_width(t),th=vita2d_texture_get_height(t);if(dm_current_window){int ox=x,oy=y,ow=w,oh=h,l=dm_current_window->x,top=dm_current_window->y,r=l+dm_current_window->w,b=top+dm_current_window->h,x2=x+w,y2=y+h;if(x<l)x=l;if(y<top)y=top;if(x2>r)x2=r;if(y2>b)y2=b;if(x2<=x||y2<=y)return;unsigned sx=(unsigned)((uint64_t)(x-ox)*tw/(unsigned)ow),sy=(unsigned)((uint64_t)(y-oy)*th/(unsigned)oh),sw=(unsigned)((uint64_t)(x2-x)*tw/(unsigned)ow),sh=(unsigned)((uint64_t)(y2-y)*th/(unsigned)oh);if(sw&&sh)vita2d_draw_texture_part_scale(t,x,y,sx,sy,sw,sh,(float)(x2-x)/sw,(float)(y2-y)/sh);return;}vita2d_draw_texture_scale(t,x,y,(float)w/tw,(float)h/th);}
void dm_image_size(void*t,unsigned*w,unsigned*h){*w=t?vita2d_texture_get_width(t):0;*h=t?vita2d_texture_get_height(t):0;}
void dm_image_free(void*t){if(t){dm_memory_image_free(t);vita2d_free_texture(t);}}
typedef struct{unsigned char*data;size_t length;} Encoded;
static void encoded(png_structp png,png_bytep bytes,png_size_t count){Encoded*e=png_get_io_ptr(png);if(count>16*1024*1024-e->length)png_error(png,"Image too large");void*p=realloc(e->data,e->length+count);if(!p)png_error(png,"Memory");e->data=p;memcpy(e->data+e->length,bytes,count);e->length+=count;}
int dm_image_save_png(const char*path,unsigned w,unsigned h,const uint32_t*pixels,int exclusive){
 if(!w||!h||w>4096||h>4096||!pixels)return -1;
 png_structp png=png_create_write_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);if(!png)return -1;
 png_infop info=png_create_info_struct(png);Encoded*e=calloc(1,sizeof(*e));if(!info||!e){png_destroy_write_struct(&png,&info);free(e);return -1;}
 if(setjmp(png_jmpbuf(png))){free(e->data);free(e);png_destroy_write_struct(&png,&info);return -1;}
 png_set_write_fn(png,e,encoded,NULL);png_set_IHDR(png,info,w,h,8,PNG_COLOR_TYPE_RGBA,PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_DEFAULT,PNG_FILTER_TYPE_DEFAULT);png_write_info(png,info);
 for(unsigned y=0;y<h;y++){png_write_row(png,(png_bytep)(pixels+y*w));}
 png_write_end(png,info);
 int result=dm_fs_write(path,e->data,e->length,exclusive);free(e->data);free(e);png_destroy_write_struct(&png,&info);return result;
}
