/* Decoder tests without SDL/GPU: stream I/O, Adam7, palette/alpha, JPEG and damaged inputs. */
#include "../file_icons.h"
#include <vita2d.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <png.h>
#include <stdio.h>
#include <jpeglib.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
struct Texture {unsigned width,height;uint32_t*pixels;};
static int texture_live;
int sceIoOpen(const char*p,int flags,int mode){return open(p,flags,mode);}
int sceIoRead(int fd,void*p,unsigned count){return (int)read(fd,p,count);}
int sceIoClose(int fd){return close(fd);}
int sceIoGetstat(const char*p,SceIoStat*out){struct stat s;if(stat(p,&s))return -1;memset(out,0,sizeof(*out));out->st_size=s.st_size;out->st_mode=s.st_mode;out->st_modified=(uint64_t)s.st_mtim.tv_sec*1000000000+s.st_mtim.tv_nsec;return 0;}
void dm_rect(int x,int y,int w,int h,unsigned color){(void)x;(void)y;(void)w;(void)h;(void)color;}
vita2d_texture*vita2d_create_empty_texture(unsigned w,unsigned h){vita2d_texture*t=calloc(1,sizeof(*t));assert(t);t->width=w;t->height=h;t->pixels=calloc(w*h,4);assert(t->pixels);texture_live++;return t;}
void vita2d_free_texture(vita2d_texture*t){if(t){free(t->pixels);free(t);texture_live--;}}
unsigned vita2d_texture_get_stride(vita2d_texture*t){return t->width*4;}
void*vita2d_texture_get_datap(vita2d_texture*t){return t->pixels;}
void vita2d_draw_texture_scale(vita2d_texture*t,float x,float y,float sx,float sy){(void)t;(void)x;(void)y;(void)sx;(void)sy;}
static void fixture_png(const char*path,int palette,int interlace){FILE*f=fopen(path,"wb");assert(f);png_structp p=png_create_write_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);png_infop info=png_create_info_struct(p);assert(p&&info);assert(!setjmp(png_jmpbuf(p)));png_init_io(p,f);png_set_IHDR(p,info,130,70,8,palette?PNG_COLOR_TYPE_PALETTE:PNG_COLOR_TYPE_RGBA,interlace?PNG_INTERLACE_ADAM7:PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_DEFAULT,PNG_FILTER_TYPE_DEFAULT);png_color colors[2]={{210,40,60},{30,80,200}};unsigned char alpha[2]={255,50};if(palette){png_set_PLTE(p,info,colors,2);png_set_tRNS(p,info,alpha,2,NULL);}unsigned char pixels[70][130*4];png_bytep rows[70];for(int y=0;y<70;y++){rows[y]=pixels[y];for(int x=0;x<130;x++){if(palette)pixels[y][x]=(x>=65);else{pixels[y][x*4]=x>=65?30:210;pixels[y][x*4+1]=x>=65?80:40;pixels[y][x*4+2]=x>=65?200:60;pixels[y][x*4+3]=x>=65?50:255;}}}png_set_rows(p,info,rows);png_write_png(p,info,PNG_TRANSFORM_IDENTITY,NULL);png_destroy_write_struct(&p,&info);fclose(f);}
static void fixture_jpeg(const char*path,int progressive){FILE*f=fopen(path,"wb");assert(f);struct jpeg_compress_struct j={0};struct jpeg_error_mgr error;j.err=jpeg_std_error(&error);jpeg_create_compress(&j);jpeg_stdio_dest(&j,f);j.image_width=130;j.image_height=70;j.input_components=3;j.in_color_space=JCS_RGB;jpeg_set_defaults(&j);jpeg_set_quality(&j,95,TRUE);if(progressive)jpeg_simple_progression(&j);jpeg_start_compress(&j,TRUE);unsigned char row[130*3];for(int x=0;x<130;x++){row[x*3]=210;row[x*3+1]=40;row[x*3+2]=60;}while(j.next_scanline<j.image_height){JSAMPROW p=row;jpeg_write_scanlines(&j,&p,1);}jpeg_finish_compress(&j);jpeg_destroy_compress(&j);fclose(f);}
static unsigned random_state=19;
static unsigned random_value(void){random_state=random_state*1664525u+1013904223u;return random_state;}
static void damage(const char*original,const char*path){FILE*f=fopen(original,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long size=ftell(f);assert(size>0&&size<1000000);rewind(f);unsigned char*data=malloc(size),*copy=malloc(size);assert(data&&copy);assert(fread(data,1,size,f)==(size_t)size);fclose(f);uint32_t pixels[64*64];unsigned w,h;for(int i=0;i<1000;i++){memcpy(copy,data,size);size_t length=size;if(i%3==0)length=random_value()%(size_t)size;else for(unsigned n=1+random_value()%8;n;n--)copy[random_value()%(size_t)size]^=(unsigned char)(1+random_value()%255);f=fopen(path,"wb");assert(f);assert(fwrite(copy,1,length,f)==length);fclose(f);int result=dm_thumbnail_pixels(path,pixels,&w,&h);if(!result)assert(w&&h&&w<=64&&h<=64);}free(data);free(copy);}
int main(void){char root[]="/tmp/desktop-thumbnail-XXXXXX";assert(mkdtemp(root));char png[256],jpg[256],badpng[256],badjpg[256];snprintf(png,sizeof(png),"%s/test.png",root);snprintf(jpg,sizeof(jpg),"%s/test.jpg",root);snprintf(badpng,sizeof(badpng),"%s/bad.png",root);snprintf(badjpg,sizeof(badjpg),"%s/bad.jpg",root);uint32_t pixels[64*64];unsigned w,h;
for(int palette=0;palette<2;palette++)for(int interlace=0;interlace<2;interlace++){fixture_png(png,palette,interlace);assert(!dm_thumbnail_pixels(png,pixels,&w,&h));assert(w==64&&h==34);unsigned char*a=(unsigned char*)pixels,*b=(unsigned char*)(pixels+63);assert(a[0]==210&&a[1]==40&&a[2]==60&&a[3]==255);assert(b[0]==30&&b[1]==80&&b[2]==200&&b[3]==50);}
for(int progressive=0;progressive<2;progressive++){fixture_jpeg(jpg,progressive);assert(!dm_thumbnail_pixels(jpg,pixels,&w,&h));assert(w==64&&h>=34&&h<=35);unsigned char*a=(unsigned char*)pixels;assert(abs(a[0]-210)<=4&&abs(a[1]-40)<=4&&abs(a[2]-60)<=4&&a[3]==255);}
damage(png,badpng);damage(jpg,badjpg);
/* Same size replacements must invalidate a cached thumbnail, including failed decodes. */
assert(dm_file_icon(png,0,0,56));assert(dm_file_icon(jpg,0,0,56));fixture_png(png,0,0);assert(dm_file_icon(png,0,0,56));
for(int i=0;i<180;i++){char extra[256];snprintf(extra,sizeof(extra),"%s/cache-%d.png",root,i);fixture_png(extra,0,0);assert(dm_file_icon(extra,0,0,56));assert(texture_live<=160);unlink(extra);}assert(texture_live==160);dm_file_icons_clear();assert(!texture_live);
unlink(png);unlink(jpg);unlink(badpng);unlink(badjpg);rmdir(root);puts("PASS: PNG RGBA/palette/alpha/Adam7, baseline/progressive JPEG, cached replacement/eviction/cleanup, 2000 truncated/mutated images");return 0;}
