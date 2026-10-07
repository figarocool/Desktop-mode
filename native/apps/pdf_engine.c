#include "pdf_engine.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#ifdef DM_PDF_POPPLER
#include <glib.h>
#include <glib-object.h>
#include <cairo.h>
/* Public Poppler GLib ABI, only opaque types; the installed runtime lacks development headers. */
typedef struct _PopplerDocument PopplerDocument;
typedef struct _PopplerPage PopplerPage;
extern PopplerDocument*poppler_document_new_from_bytes(GBytes*,const char*,GError**);
extern int poppler_document_get_n_pages(PopplerDocument*);
extern PopplerPage*poppler_document_get_page(PopplerDocument*,int);
extern void poppler_page_get_size(PopplerPage*,double*,double*);
extern void poppler_page_render(PopplerPage*,cairo_t*);
struct PdfEngine {PopplerDocument*document;};
PdfEngine*pdf_engine_open(unsigned char*data,size_t length,const char*password,char*error,size_t capacity){
 GBytes*bytes=g_bytes_new_static(data,length);GError*err=NULL;
 PopplerDocument*doc=poppler_document_new_from_bytes(bytes,password,&err);g_bytes_unref(bytes);
 if(!doc){snprintf(error,capacity,"%s",err?err->message:"PDF non valido");if(err)g_error_free(err);return NULL;}
 PdfEngine*e=calloc(1,sizeof(*e));if(!e){g_object_unref(doc);snprintf(error,capacity,"Memoria insufficiente");return NULL;}e->document=doc;return e;
}
void pdf_engine_close(PdfEngine*e){if(e){g_object_unref(e->document);free(e);}}
int pdf_engine_pages(PdfEngine*e){return poppler_document_get_n_pages(e->document);}
int pdf_engine_render(PdfEngine*e,int number,int zoom,int ox,int oy,unsigned width,unsigned height,uint32_t*pixels,int*maxx,int*maxy,char*error,size_t capacity){
 PopplerPage*page=poppler_document_get_page(e->document,number);if(!page){snprintf(error,capacity,"Pagina non disponibile");return -1;}
 double pw,ph;poppler_page_get_size(page,&pw,&ph);if(!isfinite(pw)||!isfinite(ph)||pw<=0||ph<=0){g_object_unref(page);snprintf(error,capacity,"Dimensioni pagina non valide");return -1;}
 double scale=fmin(width/pw,height/ph)*zoom/100.;double rw=pw*scale,rh=ph*scale;
 if(!isfinite(rw)||!isfinite(rh)||rw>100000||rh>100000){g_object_unref(page);return -1;}
 *maxx=rw>width?(int)ceil(rw-width):0;*maxy=rh>height?(int)ceil(rh-height):0;if(ox>*maxx)ox=*maxx;if(oy>*maxy)oy=*maxy;
 cairo_surface_t*s=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,width,height);cairo_t*cr=cairo_create(s);
 if(cairo_status(cr)!=CAIRO_STATUS_SUCCESS){snprintf(error,capacity,"Memoria insufficiente");cairo_destroy(cr);cairo_surface_destroy(s);g_object_unref(page);return -1;}
 cairo_set_source_rgb(cr,.86,.88,.9);cairo_paint(cr);cairo_translate(cr,fmax(0,(width-rw)/2)-ox,fmax(0,(height-rh)/2)-oy);cairo_scale(cr,scale,scale);cairo_set_source_rgb(cr,1,1,1);cairo_rectangle(cr,0,0,pw,ph);cairo_fill(cr);poppler_page_render(page,cr);cairo_surface_flush(s);
 int result=cairo_status(cr)==CAIRO_STATUS_SUCCESS?0:-1;const unsigned char*data=cairo_image_surface_get_data(s);int stride=cairo_image_surface_get_stride(s);
 if(!result)for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++){uint32_t argb;memcpy(&argb,data+y*stride+x*4,4);pixels[y*width+x]=(argb&0xff000000)|((argb>>16)&255)|(argb&0x0000ff00)|((argb&255)<<16);}
 else snprintf(error,capacity,"Rendering PDF fallito");
 cairo_destroy(cr);cairo_surface_destroy(s);g_object_unref(page);return result;
}
#else
#include <mupdf/fitz.h>
struct PdfEngine {fz_context*ctx;fz_document*document;};
PdfEngine*pdf_engine_open(unsigned char*data,size_t length,const char*password,char*error,size_t capacity){
 PdfEngine*e=calloc(1,sizeof(*e));if(!e){snprintf(error,capacity,"Memoria insufficiente");return NULL;}e->ctx=fz_new_context(NULL,NULL,8*1024*1024);if(!e->ctx){free(e);snprintf(error,capacity,"Memoria insufficiente");return NULL;}
 fz_stream*stream=NULL;int failed=0;fz_var(stream);fz_var(failed);
 fz_try(e->ctx){fz_register_document_handlers(e->ctx);stream=fz_open_memory(e->ctx,data,length);e->document=fz_open_document_with_stream(e->ctx,"application/pdf",stream);if(fz_needs_password(e->ctx,e->document)&&(!password||!fz_authenticate_password(e->ctx,e->document,password))){snprintf(error,capacity,"Password richiesta o non corretta");failed=1;}}
 fz_always(e->ctx){fz_drop_stream(e->ctx,stream);}
 fz_catch(e->ctx){snprintf(error,capacity,"%s",fz_caught_message(e->ctx));failed=1;}
 if(failed){pdf_engine_close(e);return NULL;}return e;
}
void pdf_engine_close(PdfEngine*e){if(e){fz_drop_document(e->ctx,e->document);fz_drop_context(e->ctx);free(e);}}
int pdf_engine_pages(PdfEngine*e){int n=0;fz_try(e->ctx){n=fz_count_pages(e->ctx,e->document);}fz_catch(e->ctx){n=0;}return n;}
int pdf_engine_render(PdfEngine*e,int number,int zoom,int ox,int oy,unsigned width,unsigned height,uint32_t*pixels,int*maxx,int*maxy,char*error,size_t capacity){
 fz_page*page=NULL;fz_pixmap*pix=NULL;fz_device*dev=NULL;int result=-1;fz_var(page);fz_var(pix);fz_var(dev);fz_var(result);
 fz_try(e->ctx){
  page=fz_load_page(e->ctx,e->document,number);fz_rect bounds=fz_bound_page(e->ctx,page);float pw=bounds.x1-bounds.x0,ph=bounds.y1-bounds.y0;
  if(!isfinite(pw)||!isfinite(ph)||pw<=0||ph<=0)fz_throw(e->ctx,FZ_ERROR_FORMAT,"Dimensioni pagina non valide");
  float scale=fminf(width/pw,height/ph)*zoom/100.f,rw=pw*scale,rh=ph*scale;if(!isfinite(rw)||!isfinite(rh)||rw>100000||rh>100000)fz_throw(e->ctx,FZ_ERROR_FORMAT,"Pagina troppo grande");
  *maxx=rw>width?(int)ceilf(rw-width):0;*maxy=rh>height?(int)ceilf(rh-height):0;if(ox>*maxx)ox=*maxx;if(oy>*maxy)oy=*maxy;
  fz_matrix matrix={scale,0,0,scale,fmaxf(0,(width-rw)/2)-ox-bounds.x0*scale,fmaxf(0,(height-rh)/2)-oy-bounds.y0*scale};
  pix=fz_new_pixmap_with_bbox(e->ctx,fz_device_rgb(e->ctx),(fz_irect){0,0,width,height},NULL,0);fz_clear_pixmap_with_value(e->ctx,pix,255);dev=fz_new_draw_device(e->ctx,fz_identity,pix);fz_run_page(e->ctx,page,dev,matrix,NULL);fz_close_device(e->ctx,dev);
  unsigned char*data=fz_pixmap_samples(e->ctx,pix);int stride=fz_pixmap_stride(e->ctx,pix),channels=fz_pixmap_components(e->ctx,pix);
  for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++){unsigned char*p=data+y*stride+x*channels;pixels[y*width+x]=p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|0xff000000;}
  result=0;
 }fz_always(e->ctx){fz_drop_device(e->ctx,dev);fz_drop_pixmap(e->ctx,pix);fz_drop_page(e->ctx,page);}
 fz_catch(e->ctx){snprintf(error,capacity,"%s",fz_caught_message(e->ctx));}
 return result;
}
#endif
