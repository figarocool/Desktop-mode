#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../apps/pdf_engine.h"
int main(int argc,char**argv){
 assert(argc==2||argc==3);FILE*f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);long length=ftell(f);rewind(f);unsigned char*data=malloc(length);assert(data&&fread(data,1,length,f)==(size_t)length);fclose(f);char error[180]={0};PdfEngine*e=pdf_engine_open(data,length,NULL,error,sizeof(error));if(!e){fprintf(stderr,"OPEN: %s\n",error);free(data);return 1;}assert(pdf_engine_pages(e)>0);uint32_t*pixels=malloc(960*544*4);assert(pixels);int mx,my;int status=pdf_engine_render(e,0,100,0,0,960,544,pixels,&mx,&my,error,sizeof(error));if(status){fprintf(stderr,"RENDER: %s\n",error);pdf_engine_close(e);free(data);free(pixels);return 1;}int dark=0,colored=0,green_pixels=0;for(int i=0;i<960*544;i++){unsigned red=pixels[i]&255,green=pixels[i]>>8&255,blue=pixels[i]>>16&255;if(red<100&&green<100&&blue<100)dark++;if(red>200&&green<100&&blue<100)colored++;if(green>200&&red<100&&blue<100)green_pixels++;}assert(dark>100&&colored>100);if(argc==3)assert(green_pixels>100);for(int i=0;i<10;i++)assert(!pdf_engine_render(e,0,100+i*25,20,20,700,400,pixels,&mx,&my,error,sizeof(error)));printf("PASS: native PDF real text/vector pixels, compressed streams, font embedding, repeated zoom/viewport (%d dark, %d red)\n",dark,colored);pdf_engine_close(e);free(data);free(pixels);return 0;
}
