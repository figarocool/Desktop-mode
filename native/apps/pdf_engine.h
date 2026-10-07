#ifndef DM_PDF_ENGINE_H
#define DM_PDF_ENGINE_H
#include <stddef.h>
#include <stdint.h>
typedef struct PdfEngine PdfEngine;
/* Input memory must remain valid until close. */
PdfEngine*pdf_engine_open(unsigned char*,size_t,const char*password,char*error,size_t capacity);
void pdf_engine_close(PdfEngine*);
int pdf_engine_pages(PdfEngine*);
/* Render only the viewport, keeping allocations bounded while zooming. */
int pdf_engine_render(PdfEngine*,int page,int zoom,int offset_x,int offset_y,unsigned width,unsigned height,uint32_t*pixels,int*max_x,int*max_y,char*error,size_t capacity);
#endif
