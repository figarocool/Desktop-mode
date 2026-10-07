/* Reproducible malformed-input regression, no user files involved. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../apps/pdf_engine.h"
static unsigned seed=12345;
static unsigned random_byte(void){seed=seed*1664525+1013904223;return seed>>24;}
int main(int argc,char**argv){assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);unsigned char*original=malloc(n),*bytes=malloc(n);assert(original&&bytes&&fread(original,1,n,f)==(size_t)n);fclose(f);uint32_t*pixels=malloc(320*240*4);assert(pixels);char error[180];for(int iteration=0;iteration<250;iteration++){memcpy(bytes,original,n);size_t size=iteration<100?(size_t)n*iteration/100:(size_t)n;for(int j=0;j<iteration%7;j++)bytes[(unsigned)random_byte()*n/256]=random_byte();PdfEngine*e=pdf_engine_open(bytes,size,NULL,error,sizeof(error));if(e){int x,y;pdf_engine_render(e,0,100,0,0,320,240,pixels,&x,&y,error,sizeof(error));pdf_engine_close(e);}}free(pixels);free(bytes);free(original);puts("PASS: 250 deterministic truncated/mutated PDF inputs under sanitizers");}
