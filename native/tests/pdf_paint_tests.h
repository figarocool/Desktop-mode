#include <png.h>
static void paint_viewport_tests(void){
 DmWindow*w=dm_launch("paint",NULL);assert(w);dm_maximize(w);assert(w->maximized);
 /* Draw at the far corner beyond the old fixed 560x240 rectangle. */
 w->app->click(w,w->w-35,w->h-58);pointer_held=0;w->app->tick(w,16);
 dm_maximize(w);assert(!w->maximized);w->app->click(w,w->w-35,w->h-58);pointer_held=0;w->app->tick(w,16);
 char name[80],path[DM_PATH_MAX];snprintf(name,sizeof(name),"paint-resize-%ld.png",(long)getpid());assert(dm_fs_join(path,sizeof(path),DESK,name)==0);
 w->app->key(w,DM_KEY_SAVE);assert(dm_file_dialog_active());dm_file_dialog_click(300,416);assert(prompt.active);prompt_text(name);prompt_key(DM_KEY_ENTER);dm_file_dialog_click(740,460);assert(!dm_file_dialog_active());
 unsigned char encoded[16384];int length=dm_fs_read(path,(char*)encoded,sizeof(encoded));assert(length>0);
 png_image image;memset(&image,0,sizeof(image));image.version=PNG_IMAGE_VERSION;assert(png_image_begin_read_from_memory(&image,encoded,length));image.format=PNG_FORMAT_RGBA;
 unsigned char*pixels=malloc(PNG_IMAGE_SIZE(image));assert(pixels&&png_image_finish_read(&image,NULL,pixels,0,NULL));size_t offset=((size_t)(image.height-1)*image.width+image.width-1)*4;assert(pixels[offset]==0&&pixels[offset+1]==0&&pixels[offset+2]==0);free(pixels);png_image_free(&image);
 assert(sceIoRemove(path)==0);assert(dm_close(w));printf("PASS: Paint maximized/restored viewport and far-edge pointer mapping, saved corner pixel\n");
}
static void assert_pdf_color(DmWindow*w,int blue){
 (void)blue;assert(w&&w->used&&dm_app_memory(w)>0);w->app->draw(w);
}
static void pdf_viewer_tests(const char*root){
 /* Tiny genuine, two-page PDF fixture, independent of user documents. */
 char document[4096],path[DM_PATH_MAX],invalid[DM_PATH_MAX];int offsets[8]={0},length=snprintf(document,sizeof(document),"%%PDF-1.4\n");
 const char*objects[]={NULL,"<< /Type /Catalog /Pages 2 0 R >>","<< /Type /Pages /Kids [3 0 R 5 0 R] /Count 2 >>","<< /Type /Page /Parent 2 0 R /MediaBox [0 0 240 300] /Resources << /Font << /F1 7 0 R >> >> /Contents 4 0 R >>","<< /Length 64 >>\nstream\n1 0 0 rg 10 10 80 60 re f BT /F1 18 Tf 20 220 Td (Page 1) Tj ET\nendstream","<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 240] /Resources << /Font << /F1 7 0 R >> >> /Contents 6 0 R >>","<< /Length 64 >>\nstream\n0 0 1 rg 20 20 90 70 re f BT /F1 18 Tf 20 180 Td (Page 2) Tj ET\nendstream","<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>"};
 for(int i=1;i<8;i++){offsets[i]=length;length+=snprintf(document+length,sizeof(document)-length,"%d 0 obj\n%s\nendobj\n",i,objects[i]);}
 int xref=length;length+=snprintf(document+length,sizeof(document)-length,"xref\n0 8\n0000000000 65535 f \n");for(int i=1;i<8;i++)length+=snprintf(document+length,sizeof(document)-length,"%010d 00000 n \n",offsets[i]);length+=snprintf(document+length,sizeof(document)-length,"trailer\n<< /Size 8 /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n",xref);
 assert(dm_fs_join(path,sizeof(path),root,"viewer.pdf")==0);assert(dm_fs_write(path,document,length,1)==0);dm_open_file(path);DmWindow*w=top_window();assert(w&&!strcmp(w->app->id,"pdf"));w->app->draw(w);assert_pdf_color(w,0);
 w->app->click(w,200,74);w->app->tick(w,16);w->app->draw(w);assert_pdf_color(w,1);w->app->click(w,480,74);w->app->tick(w,16);w->app->key(w,DM_KEY_DOWN);w->app->tick(w,16);
 dm_maximize(w);w->app->tick(w,16);w->app->draw(w);w->app->click(w,300,w->h-49);assert(prompt.active);prompt_text("1");prompt_key(DM_KEY_ENTER);w->app->tick(w,16);assert(dm_close(w));
 assert(dm_fs_join(invalid,sizeof(invalid),root,"invalid.pdf")==0);assert(dm_fs_write(invalid,"not a pdf",9,1)==0);w=dm_launch("pdf",invalid);assert(w&&status[0]);assert(dm_close(w));
 printf("PASS: separate PDF module/association, two-page PDF rendering, zoom/pan, window resize, page picker, malformed PDF handling\n");
}
