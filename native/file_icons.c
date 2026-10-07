#include "file_icons.h"
#include <vita2d.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <png.h>
#include <stdio.h>
#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#define THUMB 64
#define CACHE 160
#define INK DM_COLOR(50,97,151,255)
#define PAPER DM_COLOR(249,251,250,255)
static void dimensions(unsigned w,unsigned h,unsigned*ow,unsigned*oh){if(w<=THUMB&&h<=THUMB){*ow=w;*oh=h;return;}if(w>=h){*ow=THUMB;*oh=(unsigned)((uint64_t)h*THUMB/w);}else{*oh=THUMB;*ow=(unsigned)((uint64_t)w*THUMB/h);}if(!*ow)*ow=1;if(!*oh)*oh=1;}
static int valid_dimensions(unsigned w,unsigned h){return w&&h&&w<=16384&&h<=16384&&(uint64_t)w*h<=64000000;}
static void png_read_file(png_structp png,png_bytep bytes,png_size_t count){int fd=*(int*)png_get_io_ptr(png);while(count){int got=sceIoRead(fd,bytes,count);if(got<=0)png_error(png,"Read failed");bytes+=got;count-=(unsigned)got;}}
static void png_quiet(png_structp p,png_const_charp message){(void)message;longjmp(png_jmpbuf(p),1);}
static void png_warning_quiet(png_structp p,png_const_charp message){(void)p;(void)message;}
typedef struct {png_structp png;png_infop info;unsigned char*rows,*scratch;int fd;unsigned w,h,ow,oh;uint32_t*out;} Png;
static int png_thumbnail(int fd,uint32_t*out,unsigned*width,unsigned*height){Png*c=calloc(1,sizeof(*c));if(!c)return -1;c->fd=fd;c->out=out;c->png=png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,png_quiet,png_warning_quiet);if(!c->png){free(c);return -1;}c->info=png_create_info_struct(c->png);if(!c->info){png_destroy_read_struct(&c->png,NULL,NULL);free(c);return -1;}volatile int result=-1;if(setjmp(png_jmpbuf(c->png)))goto done;png_set_read_fn(c->png,&c->fd,png_read_file);png_set_user_limits(c->png,16384,16384);png_read_info(c->png,c->info);c->w=png_get_image_width(c->png,c->info);c->h=png_get_image_height(c->png,c->info);if(!valid_dimensions(c->w,c->h))goto done;dimensions(c->w,c->h,&c->ow,&c->oh);int depth=png_get_bit_depth(c->png,c->info),color=png_get_color_type(c->png,c->info),alpha=(color&PNG_COLOR_MASK_ALPHA)||png_get_valid(c->png,c->info,PNG_INFO_tRNS);if(depth==16)png_set_strip_16(c->png);if(color==PNG_COLOR_TYPE_PALETTE)png_set_palette_to_rgb(c->png);if(color==PNG_COLOR_TYPE_GRAY&&depth<8)png_set_expand_gray_1_2_4_to_8(c->png);if(png_get_valid(c->png,c->info,PNG_INFO_tRNS))png_set_tRNS_to_alpha(c->png);if(color==PNG_COLOR_TYPE_GRAY||color==PNG_COLOR_TYPE_GRAY_ALPHA)png_set_gray_to_rgb(c->png);if(!alpha)png_set_add_alpha(c->png,255,PNG_FILLER_AFTER);int passes=png_set_interlace_handling(c->png);png_read_update_info(c->png,c->info);size_t stride=png_get_rowbytes(c->png,c->info);if(stride!=(size_t)c->w*4)goto done;
/* Retain only the sampled rows across Adam7 passes, at most 4 MiB. */
c->rows=calloc(c->oh,stride);c->scratch=malloc(stride);if(!c->rows||!c->scratch)goto done;for(int pass=0;pass<passes;pass++){unsigned sample=0;for(unsigned y=0;y<c->h;y++){unsigned target=(unsigned)((uint64_t)sample*c->h/c->oh);unsigned char*row=sample<c->oh&&y==target?c->rows+(size_t)sample++*stride:c->scratch;if(row==c->scratch)memset(row,0,stride);png_read_row(c->png,row,NULL);}}png_read_end(c->png,NULL);for(unsigned y=0;y<c->oh;y++)for(unsigned x=0;x<c->ow;x++)memcpy(out+y*c->ow+x,c->rows+(size_t)y*stride+(size_t)((uint64_t)x*c->w/c->ow)*4,4);*width=c->ow;*height=c->oh;result=0;
done:free(c->rows);free(c->scratch);png_destroy_read_struct(&c->png,&c->info,NULL);free(c);return result;}
typedef struct {struct jpeg_error_mgr error;jmp_buf jump;} Error;
typedef struct {struct jpeg_source_mgr source;int fd;JOCTET buffer[8192];} Source;
static void jpeg_failure(j_common_ptr j){longjmp(((Error*)j->err)->jump,1);}
static void jpeg_quiet(j_common_ptr j){(void)j;}
static void source_init(j_decompress_ptr j){(void)j;}
static boolean source_fill(j_decompress_ptr j){Source*s=(Source*)j->src;int n=sceIoRead(s->fd,s->buffer,sizeof(s->buffer));if(n<=0)ERREXIT(j,JERR_INPUT_EOF);s->source.next_input_byte=s->buffer;s->source.bytes_in_buffer=n;return TRUE;}
static void source_skip(j_decompress_ptr j,long n){if(n<=0)return;while((unsigned long)n>j->src->bytes_in_buffer){n-=(long)j->src->bytes_in_buffer;source_fill(j);}j->src->next_input_byte+=n;j->src->bytes_in_buffer-=n;}
static void source_end(j_decompress_ptr j){(void)j;}
typedef struct {struct jpeg_decompress_struct jpeg;Error error;Source source;unsigned char*row;int created;} Jpeg;
static int jpeg_thumbnail(int fd,uint32_t*out,unsigned*width,unsigned*height){Jpeg*c=calloc(1,sizeof(*c));if(!c)return -1;volatile int result=-1;c->jpeg.err=jpeg_std_error(&c->error.error);c->error.error.error_exit=jpeg_failure;c->error.error.output_message=jpeg_quiet;if(setjmp(c->error.jump))goto done;jpeg_create_decompress(&c->jpeg);c->created=1;c->source.fd=fd;c->source.source.init_source=source_init;c->source.source.fill_input_buffer=source_fill;c->source.source.skip_input_data=source_skip;c->source.source.resync_to_restart=jpeg_resync_to_restart;c->source.source.term_source=source_end;c->jpeg.src=&c->source.source;jpeg_read_header(&c->jpeg,TRUE);if(!valid_dimensions(c->jpeg.image_width,c->jpeg.image_height))goto done;
/* Progressive JPEG keeps coefficient arrays: cap those sources to 16 MP. */
if(c->jpeg.progressive_mode&&(uint64_t)c->jpeg.image_width*c->jpeg.image_height>16000000){goto done;}
c->jpeg.scale_num=1;unsigned longest=c->jpeg.image_width>c->jpeg.image_height?c->jpeg.image_width:c->jpeg.image_height;c->jpeg.scale_denom=longest>=512?8:longest>=256?4:longest>=128?2:1;c->jpeg.out_color_space=JCS_RGB;jpeg_start_decompress(&c->jpeg);unsigned w=c->jpeg.output_width,h=c->jpeg.output_height,ow,oh;dimensions(w,h,&ow,&oh);if(c->jpeg.output_components!=3)goto done;c->row=malloc((size_t)w*3);if(!c->row)goto done;unsigned sample=0;while(c->jpeg.output_scanline<h){unsigned y=c->jpeg.output_scanline;JSAMPROW row=c->row;if(jpeg_read_scanlines(&c->jpeg,&row,1)!=1)goto done;while(sample<oh&&(unsigned)((uint64_t)sample*h/oh)==y){for(unsigned x=0;x<ow;x++){unsigned sx=(unsigned)((uint64_t)x*w/ow);unsigned char*p=(unsigned char*)(out+sample*ow+x);p[0]=row[sx*3];p[1]=row[sx*3+1];p[2]=row[sx*3+2];p[3]=255;}sample++;}}jpeg_finish_decompress(&c->jpeg);*width=ow;*height=oh;result=0;
done:free(c->row);if(c->created)jpeg_destroy_decompress(&c->jpeg);free(c);return result;}
int dm_thumbnail_pixels(const char*path,uint32_t*out,unsigned*w,unsigned*h){if(!path||!out||!w||!h)return -1;*w=*h=0;const char*e=strrchr(path,'.');if(!e)return -1;SceIoStat stat;if(sceIoGetstat(path,&stat)<0||stat.st_size>64*1024*1024||SCE_S_ISDIR(stat.st_mode))return -1;int fd=sceIoOpen(path,SCE_O_RDONLY,0);if(fd<0)return -1;volatile int result=-1;if(!dm_ascii_casecmp(e,".png"))result=png_thumbnail(fd,out,w,h);else if(!dm_ascii_casecmp(e,".jpg")||!dm_ascii_casecmp(e,".jpeg"))result=jpeg_thumbnail(fd,out,w,h);sceIoClose(fd);return result;}
typedef struct {char path[DM_PATH_MAX];vita2d_texture*texture;unsigned w,h;uint64_t used;SceIoStat stat;int present;} Cached;
static Cached cache[CACHE];static uint64_t serial;
static int same_time(const SceIoStat*a,const SceIoStat*b){
#ifdef DESKTOP_PREVIEW
 return a->st_modified==b->st_modified;
#else
 return !memcmp(&a->st_mtime,&b->st_mtime,sizeof(a->st_mtime));
#endif
}

void dm_file_icons_clear(void){for(int i=0;i<CACHE;i++){if(cache[i].texture)vita2d_free_texture(cache[i].texture);memset(&cache[i],0,sizeof(cache[i]));}}
static Cached*thumbnail(const char*path){SceIoStat stat;memset(&stat,0,sizeof(stat));int present=sceIoGetstat(path,&stat)>=0;int slot=0;uint64_t oldest=UINT64_MAX;for(int i=0;i<CACHE;i++){if(!strcmp(cache[i].path,path)){slot=i;if(cache[i].present==present&&cache[i].stat.st_size==stat.st_size&&same_time(&cache[i].stat,&stat)){cache[i].used=++serial;return cache+i;}break;}if(cache[i].used<oldest){oldest=cache[i].used;slot=i;}}Cached*c=cache+slot;if(c->texture)vita2d_free_texture(c->texture);memset(c,0,sizeof(*c));snprintf(c->path,sizeof(c->path),"%s",path);c->used=++serial;c->present=present;c->stat=stat;uint32_t pixels[THUMB*THUMB];if(present&&!dm_thumbnail_pixels(path,pixels,&c->w,&c->h)){c->texture=vita2d_create_empty_texture(c->w,c->h);if(c->texture){unsigned stride=vita2d_texture_get_stride(c->texture);unsigned char*p=vita2d_texture_get_datap(c->texture);for(unsigned y=0;y<c->h;y++)memcpy(p+y*stride,pixels+y*c->w,c->w*4);}}return c;}
static void document(int x,int y,int s){int left=x+s/6,top=y+s/12,width=s*2/3,height=s*5/6;dm_rect(left+2,top+2,width,height,DM_COLOR(30,47,65,90));dm_rect(left,top,width,height,INK);dm_rect(left+s/12,top+s/18,width-s/9,height-s/9,PAPER);dm_rect(left,top+s/14,s/9,height-s/7,DM_COLOR(78,145,205,255));for(int i=0;i<5;i++){int yy=top+s/5+i*s/10;dm_rect(left+s/6,yy,width-s/4,s>40?2:1,DM_COLOR(125,149,168,255));}for(int i=0;i<4;i++)dm_rect(left-1,top+s/8+i*s/6,s/7,s>40?3:2,DM_COLOR(193,207,214,255));}
static int media_file(const char*e){return !dm_ascii_casecmp(e,".mp3")||!dm_ascii_casecmp(e,".wav")||!dm_ascii_casecmp(e,".mid")||!dm_ascii_casecmp(e,".midi")||!dm_ascii_casecmp(e,".rmi")||!dm_ascii_casecmp(e,".m4a")||!dm_ascii_casecmp(e,".mp4")||!dm_ascii_casecmp(e,".aif")||!dm_ascii_casecmp(e,".aiff")||!dm_ascii_casecmp(e,".aifc")||!dm_ascii_casecmp(e,".au")||!dm_ascii_casecmp(e,".snd")||!dm_ascii_casecmp(e,".voc");}
static int media_icon(int x,int y,int s,const char*e){uint32_t c=DM_COLOR(49,108,197,255);if(!dm_ascii_casecmp(e,".wav"))c=DM_COLOR(41,140,134,255);else if(!dm_ascii_casecmp(e,".mid")||!dm_ascii_casecmp(e,".midi")||!dm_ascii_casecmp(e,".rmi"))c=DM_COLOR(134,84,183,255);else if(!dm_ascii_casecmp(e,".m4a"))c=DM_COLOR(68,147,201,255);else if(!dm_ascii_casecmp(e,".mp4"))c=DM_COLOR(190,89,69,255);else if(!dm_ascii_casecmp(e,".aif")||!dm_ascii_casecmp(e,".aiff")||!dm_ascii_casecmp(e,".aifc"))c=DM_COLOR(170,124,54,255);else if(!dm_ascii_casecmp(e,".au")||!dm_ascii_casecmp(e,".snd"))c=DM_COLOR(109,142,67,255);else if(!dm_ascii_casecmp(e,".voc"))c=DM_COLOR(100,115,181,255);int left=x+s/7,top=y+s/10,body=s*3/4;dm_rect(left,top,body,s*4/5,DM_COLOR(250,252,255,255));dm_rect(left,top,body,s/8,c);if(!dm_ascii_casecmp(e,".mp4")){for(int i=0;i<3;i++)dm_rect(left+s/5+i*s/7,top+s/3,s/10,s/3,c);dm_rect(left+s*3/5,top+s/3,s/8,s/3,DM_COLOR(255,255,255,255));}else{int bars=5,barw=s/13;if(barw<1)barw=1;for(int i=0;i<bars;i++){int bh=s*(2+(i*7%5))/13;dm_rect(left+s/8+i*(barw+1),top+s/2-bh/2,barw,bh,c);}}return 1;}
static int pdf_icon(int x,int y,int s){int left=x+s/7,top=y+s/10;dm_rect(left,top,s*3/4,s*4/5,DM_COLOR(255,255,255,255));dm_rect(left,top,s*3/4,s/7,DM_COLOR(190,48,48,255));for(int i=0;i<3;i++)dm_rect(left+s/7,top+s/3+i*s/8,s/2,1,DM_COLOR(115,129,145,255));return 1;}
static void photo(int x,int y,int s,Cached*c){int available=s*4/5,w=available,h=available;if(c&&c->texture){if(c->w>=c->h)h=(int)((uint64_t)available*c->h/c->w);else w=(int)((uint64_t)available*c->w/c->h);}int xx=x+(s-w)/2,yy=y+(s-h)/2;dm_rect(xx-3,yy-3,w+6,h+6,DM_COLOR(53,76,99,255));dm_rect(xx-2,yy-2,w+4,h+4,PAPER);if(c&&c->texture)dm_image_draw(c->texture,xx,yy,w,h);else{dm_rect(xx,yy,w,h,DM_COLOR(132,193,224,255));dm_rect(xx+w/8,yy+h/2,w*3/4,h/2,DM_COLOR(82,139,82,255));dm_rect(xx+w*3/4,yy+h/8,w/8,h/8,DM_COLOR(250,226,113,255));}}
int dm_file_icon(const char*path,int x,int y,int size){if(!path||size<12)return 0;const char*e=strrchr(path,'.');if(!e)return 0;if(!dm_ascii_casecmp(e,".txt")||!dm_ascii_casecmp(e,".log")||!dm_ascii_casecmp(e,".ini")||!dm_ascii_casecmp(e,".md")||!dm_ascii_casecmp(e,".json")||!dm_ascii_casecmp(e,".cfg")){document(x,y,size);return 1;}if(!dm_ascii_casecmp(e,".png")||!dm_ascii_casecmp(e,".jpg")||!dm_ascii_casecmp(e,".jpeg")){photo(x,y,size,thumbnail(path));return 1;}if(!dm_ascii_casecmp(e,".pdf"))return pdf_icon(x,y,size);if(media_file(e))return media_icon(x,y,size,e);return 0;}
