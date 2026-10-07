/* Desktop PDF: bounded PDF object parser and software viewport renderer.
 * Original implementation; format reference Adobe PDF Reference 1.7.
 * No scripts, external URLs or embedded programs are executed.
 */
#include "../apps/pdf_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <setjmp.h>
#include <zlib.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <jpeglib.h>
#define OBJECT_MAX 8192
#define PAGE_MAX 1024
#define MEMORY_MAX (40u*1024u*1024u)
#define STREAM_MAX (16u*1024u*1024u)
#define NODE_MAX 131072
#define PATH_MAXIMUM 8192
#define FAIL(e,s) fail(e,s)
typedef struct Value Value;
typedef struct Block{struct Block*next;size_t size;}Block;
struct Value{int type,id,generation;double number;unsigned char*bytes;size_t length;Value*child,*next;};
enum{NUM=1,NAME,STRING,ARRAY,DICT,REF,WORD};
typedef struct {Value*v;unsigned char*stream;size_t size;int generation;}Object;
typedef struct {unsigned char*p,*end;PdfEngine*e;int depth;}Parser;
struct PdfEngine{unsigned char*data;size_t length;Object objects[OBJECT_MAX];Value*pages[PAGE_MAX];int page_count,failed,nodes;Block*memory;size_t allocated;char message[180];FT_Library ft;FT_Face fallback;};
static void fail(PdfEngine*e,const char*s){if(!e->failed){e->failed=1;snprintf(e->message,sizeof(e->message),"%s",s);}}
static void*allocate(PdfEngine*e,size_t n){if(n>MEMORY_MAX||e->allocated>MEMORY_MAX-n){FAIL(e,"PDF: limite memoria superato");return NULL;}Block*b=calloc(1,sizeof(*b)+n);if(!b){FAIL(e,"Memoria insufficiente");return NULL;}b->size=n;b->next=e->memory;e->memory=b;e->allocated+=n;return b+1;}
static int space(int c){return c==0||c==9||c==10||c==12||c==13||c==32;}
static int delim(int c){return space(c)||strchr("()<>[]{}/%",c)!=NULL;}
static void skip(Parser*p){while(p->p<p->end){if(space(*p->p)){p->p++;continue;}if(*p->p=='%'){while(p->p<p->end&&*p->p!='\r'&&*p->p!='\n')p->p++;continue;}break;}}
static int keyword(Parser*p,const char*s){skip(p);size_t n=strlen(s);if((size_t)(p->end-p->p)<n||memcmp(p->p,s,n)||((size_t)(p->end-p->p)>n&&!delim(p->p[n])))return 0;p->p+=n;return 1;}
static int hex(int c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static int number(Parser*p,double*out){skip(p);unsigned char*start=p->p;if(start==p->end||(!strchr("+-.0123456789",*start)))return 0;unsigned char*q=start;while(q<p->end&&!delim(*q))q++;size_t n=q-start;if(!n||n>=64)return 0;char s[64],*end;memcpy(s,start,n);s[n]=0;double v=strtod(s,&end);if(end!=s+n||!isfinite(v)||fabs(v)>100000000)return 0;*out=v;p->p=q;return 1;}
static Value*parse(Parser*p);
static Value*node(Parser*p,int type){if(++p->e->nodes>NODE_MAX){FAIL(p->e,"PDF: troppi oggetti");return NULL;}Value*v=allocate(p->e,sizeof(*v));if(v)v->type=type;return v;}
static Value*parse(Parser*p){
 skip(p);if(p->p>=p->end||p->e->failed)return NULL;if(p->depth++>=64){FAIL(p->e,"PDF: struttura troppo profonda");p->depth--;return NULL;}Value*v=NULL;int c=*p->p;
 if(c=='['||(c=='<'&&p->end-p->p>=2&&p->p[1]=='<')){
  int dict=c=='<';p->p+=dict?2:1;v=node(p,dict?DICT:ARRAY);Value**tail=v?&v->child:NULL;int pairs=0;
  while(!p->e->failed){skip(p);if(p->p==p->end){FAIL(p->e,"PDF troncato");break;}if(dict?(*p->p=='>'&&p->end-p->p>=2&&p->p[1]=='>'):*p->p==']'){p->p+=dict?2:1;if(dict&&(pairs&1))FAIL(p->e,"Dizionario PDF non valido");break;}Value*n=parse(p);if(!n)break;if(dict&&!(pairs&1)&&n->type!=NAME){FAIL(p->e,"Chiave PDF non valida");break;}if(tail){*tail=n;tail=&n->next;}pairs++;}
 }else if(c=='/'||c=='('||c=='<'){
  p->p++;unsigned char*start=p->p,*q=start;size_t max=(size_t)(p->end-start);if(c=='/')while(q<p->end&&!delim(*q))q++;else if(c=='<'){while(q<p->end&&*q!='>')q++;if(q==p->end){FAIL(p->e,"Stringa PDF troncata");goto done;}}else{int level=1;while(q<p->end&&level){if(*q=='\\'){q++;if(q<p->end)q++;continue;}if(*q=='(')level++;if(*q==')')level--;if(level)q++;}if(q==p->end){FAIL(p->e,"Stringa PDF troncata");goto done;}}
  max=q-start;v=node(p,c=='/'?NAME:STRING);if(!v)goto done;v->bytes=allocate(p->e,max+1);if(!v->bytes)goto done;size_t n=0;
  while(p->p<q){int a=*p->p++;if(c=='/'&&a=='#'&&q-p->p>=2){int h=hex(p->p[0]),l=hex(p->p[1]);if(h<0||l<0){FAIL(p->e,"Nome PDF non valido");break;}a=h*16+l;p->p+=2;}else if(c=='<'){if(space(a))continue;int h=hex(a);if(h<0){FAIL(p->e,"Stringa hex PDF non valida");break;}while(p->p<q&&space(*p->p))p->p++;int l=p->p<q?hex(*p->p++):0;if(l<0){FAIL(p->e,"Stringa hex PDF non valida");break;}a=h*16+l;}else if(c=='('&&a=='\\'){if(p->p==q)break;a=*p->p++;if(a=='\n')continue;if(a=='\r'){if(p->p<q&&*p->p=='\n')p->p++;continue;}if(a=='n')a='\n';else if(a=='r')a='\r';else if(a=='t')a='\t';else if(a=='b')a='\b';else if(a=='f')a='\f';else if(a>='0'&&a<='7'){int val=a-'0';for(int k=0;k<2&&p->p<q&&*p->p>='0'&&*p->p<='7';k++)val=val*8+*p->p++-'0';a=val&255;}}
   v->bytes[n++]=a;
  }v->length=n;p->p=q+(c=='/'?0:1);
 }else{
  double n;if(number(p,&n)){v=node(p,NUM);if(!v)goto done;v->number=n;Parser look=*p;double generation;if(n>=0&&n<OBJECT_MAX&&floor(n)==n&&number(&look,&generation)&&generation>=0&&generation<=65535&&floor(generation)==generation&&keyword(&look,"R")){v->type=REF;v->id=(int)n;v->generation=(int)generation;p->p=look.p;}}
  else{unsigned char*q=p->p;while(p->p<p->end&&!delim(*p->p))p->p++;if(p->p==q){FAIL(p->e,"Token PDF non valido");goto done;}v=node(p,WORD);if(v){v->length=p->p-q;v->bytes=allocate(p->e,v->length+1);if(v->bytes)memcpy(v->bytes,q,v->length);}}
 }
 done:p->depth--;return v;
}
static int named(Value*v,const char*s){return v&&(v->type==NAME||v->type==WORD)&&v->bytes&&v->length==strlen(s)&&!memcmp(v->bytes,s,v->length);}
static Value*resolve(PdfEngine*e,Value*v){for(int k=0;v&&v->type==REF;k++){if(k>=64||v->id<=0||v->id>=OBJECT_MAX){FAIL(e,"Riferimento PDF non valido");return NULL;}Object*o=&e->objects[v->id];if(!o->v||o->generation!=v->generation){FAIL(e,"Oggetto PDF mancante");return NULL;}v=o->v;}return v;}
static Value*get(PdfEngine*e,Value*d,const char*name){d=resolve(e,d);if(!d||d->type!=DICT)return NULL;for(Value*k=d->child;k&&k->next;k=k->next->next)if(named(k,name))return k->next;return NULL;}
static double numeric(PdfEngine*e,Value*v,double fallback){v=resolve(e,v);return v&&v->type==NUM?v->number:fallback;}
static Value*at(PdfEngine*e,Value*v,int n){v=resolve(e,v);if(!v||v->type!=ARRAY)return NULL;v=v->child;while(v&&n-->0)v=v->next;return resolve(e,v);}
static unsigned char*findbytes(unsigned char*p,unsigned char*end,const char*s){size_t n=strlen(s);for(;p<=end&&n<=(size_t)(end-p);p++)if(*p==(unsigned char)*s&&!memcmp(p,s,n))return p;return NULL;}
static int index_objects(PdfEngine*e){
 Parser p={e->data,e->data+e->length,e,0};
 while(p.p<p.end&&!e->failed){skip(&p);if(p.p==p.end)break;Parser look=p;double id,gen;
  if(number(&look,&id)&&number(&look,&gen)&&keyword(&look,"obj")){
   if(id<=0||id>=OBJECT_MAX||floor(id)!=id||gen<0||gen>65535||floor(gen)!=gen){FAIL(e,"PDF: numero oggetto fuori limite");break;}Object*o=&e->objects[(int)id];o->v=parse(&look);o->generation=(int)gen;o->stream=NULL;o->size=0;if(!o->v)break;
   if(keyword(&look,"stream")){
    if(look.p<look.end&&*look.p=='\r')look.p++;
    if(look.p<look.end&&*look.p=='\n')look.p++;else{FAIL(e,"Stream PDF non valido");break;}
    unsigned char*start=look.p;Value*length=get(e,o->v,"Length");double size=length&&length->type==NUM?length->number:-1;unsigned char*end=NULL;
    if(size>=0&&size<=(double)(look.end-start)&&floor(size)==size){Parser check=look;check.p=start+(size_t)size;if(keyword(&check,"endstream")){end=start+(size_t)size;look.p=check.p;}}
    if(!end){end=findbytes(start,look.end,"endstream");if(!end){FAIL(e,"Stream PDF troncato");break;}look.p=end+9;while(end>start&&(end[-1]=='\n'||end[-1]=='\r'))end--;}
    o->stream=start;o->size=end-start;
   }
   if(!keyword(&look,"endobj")){FAIL(e,"Oggetto PDF troncato");break;}p.p=look.p;
  }else if(keyword(&look,"trailer")){Value*t=parse(&look);if(get(e,t,"Encrypt"))FAIL(e,"PDF cifrati non ancora supportati dal renderer nativo");p.p=look.p;}
  else{ /* Skip tokens without interpreting fake object headers inside strings. */
   if(*p.p=='('||*p.p=='<'||*p.p=='['||*p.p=='/')parse(&p);else{unsigned char*q=p.p;while(p.p<p.end&&!delim(*p.p))p.p++;if(p.p==q)p.p++;}
  }
 }
 return !e->failed;
}
static Object*stream_object(PdfEngine*e,Value*v){if(!v||v->type!=REF||v->id<1||v->id>=OBJECT_MAX){FAIL(e,"Stream PDF indiretto richiesto");return NULL;}Object*o=&e->objects[v->id];if(o->generation!=v->generation||!o->stream){FAIL(e,"Stream PDF mancante");return NULL;}return o;}
static unsigned char*decode(PdfEngine*e,Object*o,size_t*length,int allow_jpeg){
 if(!o)return NULL;
 Value*f=resolve(e,get(e,o->v,"Filter"));if(f&&f->type==ARRAY){if(f->child&&f->child->next){FAIL(e,"PDF: filtri concatenati non supportati");return NULL;}f=resolve(e,f->child);}*length=o->size;
 if(!f||(named(f,"DCTDecode")&&allow_jpeg))return o->stream;
 if(!named(f,"FlateDecode")){FAIL(e,"PDF: filtro di compressione non supportato");return NULL;}
 z_stream z={0};if(inflateInit(&z)!=Z_OK){FAIL(e,"Decompressione PDF non disponibile");return NULL;}size_t cap=4096,n=0;unsigned char*temp=malloc(cap);int r=Z_OK;if(!temp){inflateEnd(&z);FAIL(e,"Memoria insufficiente");return NULL;}z.next_in=o->stream;z.avail_in=o->size;
 while(r==Z_OK){if(n==cap){if(cap>=STREAM_MAX){FAIL(e,"PDF: stream decompresso troppo grande");break;}size_t next=cap*2;void*t=realloc(temp,next);if(!t){FAIL(e,"Memoria insufficiente");break;}temp=t;cap=next;}z.next_out=temp+n;z.avail_out=cap-n;r=inflate(&z,Z_NO_FLUSH);n=cap-z.avail_out;}
 inflateEnd(&z);if(r!=Z_STREAM_END&&!e->failed)FAIL(e,"PDF: stream Flate danneggiato");unsigned char*out=NULL;if(!e->failed){out=allocate(e,n+1);if(out)memcpy(out,temp,n);}free(temp);*length=n;return out;
}
static void object_streams(PdfEngine*e){for(int id=1;id<OBJECT_MAX&&!e->failed;id++){Object*o=&e->objects[id];if(!named(resolve(e,get(e,o->v,"Type")),"ObjStm"))continue;double count=numeric(e,get(e,o->v,"N"),-1),first=numeric(e,get(e,o->v,"First"),-1);size_t size;unsigned char*data=decode(e,o,&size,0);if(!data)break;if(count<0||count>OBJECT_MAX||first<0||first>size||floor(count)!=count||floor(first)!=first){FAIL(e,"Object stream PDF non valido");break;}Parser header={data,data+(size_t)first,e,0};for(int n=0;n<(int)count;n++){double index,offset;if(!number(&header,&index)||!number(&header,&offset)||index<1||index>=OBJECT_MAX||floor(index)!=index||offset<0||offset>=size-first||floor(offset)!=offset){FAIL(e,"Object stream PDF non valido");break;}if(!e->objects[(int)index].v){Parser p={data+(size_t)first+(size_t)offset,data+size,e,0};e->objects[(int)index].v=parse(&p);e->objects[(int)index].generation=0;}}}}
static void pages(PdfEngine*e,Value*v,int depth){v=resolve(e,v);if(!v||e->failed)return;if(depth>=64){FAIL(e,"Albero pagine PDF ciclico");return;}if(named(resolve(e,get(e,v,"Type")),"Page")){if(e->page_count==PAGE_MAX){FAIL(e,"PDF: troppe pagine");return;}e->pages[e->page_count++]=v;return;}Value*kids=resolve(e,get(e,v,"Kids"));if(!kids||kids->type!=ARRAY){FAIL(e,"Albero pagine PDF non valido");return;}for(Value*k=kids->child;k&&!e->failed;k=k->next)pages(e,k,depth+1);}
static Value*inherited(PdfEngine*e,Value*page,const char*name){for(int i=0;page&&i<64;i++){Value*v=get(e,page,name);if(v)return resolve(e,v);page=resolve(e,get(e,page,"Parent"));}return NULL;}
PdfEngine*pdf_engine_open(unsigned char*data,size_t length,const char*password,char*error,size_t capacity){
 (void)password;PdfEngine*e=calloc(1,sizeof(*e));if(!e){snprintf(error,capacity,"Memoria insufficiente");return NULL;}e->data=data;e->length=length;
 if(length<8||length>STREAM_MAX||memcmp(data,"%PDF-",5))FAIL(e,"PDF non valido");
 if(!e->failed)index_objects(e);
 if(!e->failed)object_streams(e);
 Value*catalog=NULL;for(int i=1;i<OBJECT_MAX&&!e->failed;i++){Value*v=e->objects[i].v;if(named(resolve(e,get(e,v,"Type")),"XRef")&&get(e,v,"Encrypt"))FAIL(e,"PDF cifrati non ancora supportati dal renderer nativo");if(named(resolve(e,get(e,v,"Type")),"Catalog"))catalog=v;}
 if(!e->failed){if(catalog)pages(e,get(e,catalog,"Pages"),0);else FAIL(e,"Catalogo PDF mancante");}if(!e->page_count&&!e->failed)FAIL(e,"PDF senza pagine");
 if(!e->failed){if(FT_Init_FreeType(&e->ft))FAIL(e,"FreeType non disponibile");else{
#ifdef DESKTOP_PREVIEW
 const char*font="native/assets/pdf-font.ttf";
#else
 const char*font="app0:/assets/pdf-font.ttf";
#endif
 if(FT_New_Face(e->ft,font,0,&e->fallback))FAIL(e,"Font PDF di base mancante");}}
 if(e->failed){snprintf(error,capacity,"%s",e->message);pdf_engine_close(e);return NULL;}return e;
}
void pdf_engine_close(PdfEngine*e){if(!e)return;if(e->fallback)FT_Done_Face(e->fallback);if(e->ft)FT_Done_FreeType(e->ft);Block*b=e->memory;while(b){Block*next=b->next;free(b);b=next;}free(e);}
int pdf_engine_pages(PdfEngine*e){return e?e->page_count:0;}
/* Rendering structures: device-space paths, affine transforms and glyph coverage. */
typedef struct{double a,b,c,d,x,y;}Matrix;
typedef struct{double x,y;int move;}Point;
typedef struct{Matrix matrix,text,line;uint32_t fill,stroke;double linewidth,font_size,charspace,wordspace,hscale,leading,rise;Value*font;int render_mode,fill_space,stroke_space;int clipx0,clipy0,clipx1,clipy1;}Graphics;
typedef struct{Value*font,*metrics;FT_Face face;unsigned*unicode;unsigned char*gid;size_t gid_size;int cid,first;Value*widths;}Font;
typedef struct{PdfEngine*e;Font fonts[16];int font_count;uint32_t*pixels;unsigned w,h;Graphics g,stack[32];int top;Point path[PATH_MAXIMUM];int points,clip;Value*resources;int depth;unsigned operations;int failed;}Render;
static Matrix identity(void){return(Matrix){1,0,0,1,0,0};}
static Matrix concat(Matrix m,Matrix n){return(Matrix){m.a*n.a+m.c*n.b,m.b*n.a+m.d*n.b,m.a*n.c+m.c*n.d,m.b*n.c+m.d*n.d,m.a*n.x+m.c*n.y+m.x,m.b*n.x+m.d*n.y+m.y};}
static Point transform(Matrix m,double x,double y){return(Point){m.a*x+m.c*y+m.x,m.b*x+m.d*y+m.y,0};}
static double clamp(double n,double low,double high){return n<low?low:n>high?high:n;}
static uint32_t color(double r,double g,double b){return 0xff000000u|(unsigned)(clamp(r,0,1)*255)|(unsigned)(clamp(g,0,1)*255)<<8|(unsigned)(clamp(b,0,1)*255)<<16;}
static void pixel(Render*r,int x,int y,uint32_t c,unsigned alpha){Graphics*g=&r->g;if(x<g->clipx0||x>=g->clipx1||y<g->clipy0||y>=g->clipy1||x<0||y<0||x>=(int)r->w||y>=(int)r->h)return;alpha=alpha*(c>>24)/255;uint32_t*dst=&r->pixels[(unsigned)y*r->w+x];if(alpha==255){*dst=c;return;}unsigned out=0xff000000;for(int i=0;i<3;i++){unsigned shift=i*8;out|=((((c>>shift)&255)*alpha+((*dst>>shift)&255)*(255-alpha))/255)<<shift;}*dst=out;}
static void point(Render*r,double x,double y,int move){if(r->points==PATH_MAXIMUM){FAIL(r->e,"PDF: percorso grafico troppo complesso");return;}Point p=transform(r->g.matrix,x,y);if(!isfinite(p.x)||!isfinite(p.y)||fabs(p.x)>1e7||fabs(p.y)>1e7){FAIL(r->e,"Coordinate PDF fuori limite");return;}p.move=move;r->path[r->points++]=p;}
static void closepath(Render*r){if(!r->points)return;int i=r->points-1;while(i>0&&!r->path[i].move)i--;if(r->points==PATH_MAXIMUM){FAIL(r->e,"Percorso PDF troppo lungo");return;}Point p=r->path[i];p.move=0;r->path[r->points++]=p;}
static void line(Render*r,Point a,Point b,uint32_t c,double width){
 /* Liang-Barsky clipping bounds work even for very distant endpoints. */
 double dx=b.x-a.x,dy=b.y-a.y,t0=0,t1=1;double p[4]={-dx,dx,-dy,dy},q[4]={a.x+17,r->w+17-a.x,a.y+17,r->h+17-a.y};
 for(int i=0;i<4;i++){if(fabs(p[i])<1e-12){if(q[i]<0)return;}else{double t=q[i]/p[i];if(p[i]<0){if(t>t1)return;t0=fmax(t0,t);}else{if(t<t0)return;t1=fmin(t1,t);}}}
 Point start={a.x+dx*t0,a.y+dy*t0,0},end={a.x+dx*t1,a.y+dy*t1,0};dx=end.x-start.x;dy=end.y-start.y;int steps=(int)ceil(hypot(dx,dy));int radius=(int)clamp(width*.5,0,16);
 for(int k=0;k<=steps;k++){double t=steps?(double)k/steps:0;int x=(int)lround(start.x+dx*t),y=(int)lround(start.y+dy*t);for(int v=-radius;v<=radius;v++)for(int u=-radius;u<=radius;u++)pixel(r,x+u,y+v,c,255);}
}
static int compare_double(const void*a,const void*b){double x=*(const double*)a,y=*(const double*)b;return(x>y)-(x<y);}
static void fillpath(Render*r,int evenodd){
 if(!r->points)return;
 double minimum=r->h,maximum=0;for(int i=0;i<r->points;i++){minimum=fmin(minimum,r->path[i].y);maximum=fmax(maximum,r->path[i].y);}int low=(int)clamp(floor(minimum),0,r->h),high=(int)clamp(ceil(maximum),0,r->h);double crossings[PATH_MAXIMUM];int winding[PATH_MAXIMUM];
 for(int y=low;y<high;y++){
  int n=0,start=0;double yy=y+.5;
  for(int i=0;i<r->points;i++){if(r->path[i].move)start=i;int j=i+1;if(j==r->points||r->path[j].move)j=start;Point a=r->path[i],b=r->path[j];if((a.y<=yy&&b.y>yy)||(b.y<=yy&&a.y>yy)){crossings[n]=a.x+(yy-a.y)*(b.x-a.x)/(b.y-a.y);winding[n++]=b.y>a.y?1:-1;}}
  /* Paired intersections implement even-odd; nonzero computes winding per interval. */
  if(evenodd){qsort(crossings,n,sizeof(double),compare_double);for(int k=0;k+1<n;k+=2){int x0=(int)clamp(ceil(crossings[k]-.5),0,r->w),x1=(int)clamp(ceil(crossings[k+1]-.5),0,r->w);for(int x=x0;x<x1;x++)pixel(r,x,y,r->g.fill,255);}}
  else{for(int k=1;k<n;k++){double x=crossings[k];int w=winding[k],j=k;while(j&&crossings[j-1]>x){crossings[j]=crossings[j-1];winding[j]=winding[j-1];j--;}crossings[j]=x;winding[j]=w;}int level=0;for(int k=0;k+1<n;k++){level+=winding[k];if(level){int x0=(int)clamp(ceil(crossings[k]-.5),0,r->w),x1=(int)clamp(ceil(crossings[k+1]-.5),0,r->w);for(int x=x0;x<x1;x++)pixel(r,x,y,r->g.fill,255);}}}
 }
}
static void stroke(Render*r){double width=r->g.linewidth*hypot(r->g.matrix.a,r->g.matrix.b);for(int i=1;i<r->points;i++)if(!r->path[i].move)line(r,r->path[i-1],r->path[i],r->g.stroke,width);}
static void curve(Render*r,double x1,double y1,double x2,double y2,double x3,double y3){if(!r->points){FAIL(r->e,"Curva PDF senza origine");return;}Point a=r->path[r->points-1],b=transform(r->g.matrix,x1,y1),c=transform(r->g.matrix,x2,y2),d=transform(r->g.matrix,x3,y3);int steps=(int)clamp(ceil((hypot(a.x-b.x,a.y-b.y)+hypot(b.x-c.x,b.y-c.y)+hypot(c.x-d.x,c.y-d.y))/4),4,128);for(int i=1;i<=steps;i++){double t=(double)i/steps,u=1-t;if(r->points==PATH_MAXIMUM){FAIL(r->e,"Percorso PDF troppo lungo");return;}r->path[r->points++]=(Point){u*u*u*a.x+3*u*u*t*b.x+3*u*t*t*c.x+t*t*t*d.x,u*u*u*a.y+3*u*u*t*b.y+3*u*t*t*c.y+t*t*t*d.y,0};}}
static unsigned unicode_byte(unsigned c){static const unsigned table[32]={0x20ac,0,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,0x2c6,0x2030,0x160,0x2039,0x152,0,0x17d,0,0,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0x2dc,0x2122,0x161,0x203a,0x153,0,0x17e,0x178};return c>=128&&c<160?table[c-128]:c;}
static unsigned string_number(Value*v){unsigned n=0;if(!v||v->type!=STRING||v->length>4)return 0;for(size_t i=0;i<v->length;i++)n=(n<<8)|v->bytes[i];return n;}
static void unicode_map(PdfEngine*e,Font*f,Value*ref){
 if(!ref)return;
 Object*o=stream_object(e,ref);size_t size;unsigned char*data=decode(e,o,&size,0);if(!data)return;f->unicode=allocate(e,65536*sizeof(unsigned));if(!f->unicode)return;Parser p={data,data+size,e,0};
 while(p.p<p.end&&!e->failed){Value*v=parse(&p);if(!v)break;if(named(v,"beginbfchar")||named(v,"beginbfrange")){int range=named(v,"beginbfrange");while(p.p<p.end&&!e->failed){Value*a=parse(&p);if(named(a,range?"endbfrange":"endbfchar"))break;Value*b=parse(&p),*c=range?parse(&p):NULL;unsigned lo=string_number(a),hi=range?string_number(b):lo;if(!a||!b||a->type!=STRING||b->type!=STRING||lo>hi||hi>=65536){FAIL(e,"CMap PDF non valido");break;}if(!range)f->unicode[lo]=string_number(b);else if(c&&c->type==STRING){unsigned first=string_number(c);for(unsigned n=lo;n<=hi;n++)f->unicode[n]=first+n-lo;}else if(c&&c->type==ARRAY){Value*entry=c->child;for(unsigned n=lo;n<=hi;n++){if(!entry){FAIL(e,"CMap PDF troncato");break;}f->unicode[n]=string_number(entry);entry=entry->next;}}else FAIL(e,"CMap PDF non supportato");}}}
}
static Font*font_get(Render*r){
 PdfEngine*e=r->e;Value*font=resolve(e,r->g.font);if(!font){FAIL(e,"Font PDF mancante");return NULL;}for(int i=0;i<r->font_count;i++)if(r->fonts[i].font==font)return &r->fonts[i];if(r->font_count==16){FAIL(e,"PDF: troppi font nella pagina");return NULL;}Font*f=&r->fonts[r->font_count++];f->font=font;f->metrics=font;f->face=e->fallback;
 Value*sub=resolve(e,get(e,font,"Subtype"));if(named(sub,"Type3")){FAIL(e,"PDF: font Type3 non ancora supportato");return NULL;}
 if(named(sub,"Type0")){f->cid=1;Value*encoding=resolve(e,get(e,font,"Encoding"));if(!named(encoding,"Identity-H")){FAIL(e,"PDF: font CID richiede Identity-H");return NULL;}f->metrics=at(e,get(e,font,"DescendantFonts"),0);if(!named(resolve(e,get(e,f->metrics,"Subtype")),"CIDFontType2")){FAIL(e,"PDF: font CID richiede TrueType CIDFontType2");return NULL;}Value*gid=get(e,f->metrics,"CIDToGIDMap");if(gid&&!named(gid,"Identity")){Object*o=stream_object(e,gid);f->gid=decode(e,o,&f->gid_size,0);}}
 Value*descriptor=resolve(e,get(e,f->metrics,"FontDescriptor")),*file=get(e,descriptor,"FontFile2");if(!file)file=get(e,descriptor,"FontFile3");if(file){Object*o=stream_object(e,file);size_t size;unsigned char*data=decode(e,o,&size,0);if(!data)return NULL;if(FT_New_Memory_Face(e->ft,data,size,0,&f->face)){FAIL(e,"Font incorporato PDF non leggibile");f->face=e->fallback;return NULL;}}
 else if(get(e,descriptor,"FontFile")){FAIL(e,"PDF: font Type1 incorporato non ancora supportato");return NULL;}
 f->first=(int)numeric(e,get(e,f->metrics,"FirstChar"),0);f->widths=resolve(e,get(e,f->metrics,f->cid?"W":"Widths"));unicode_map(e,f,get(e,font,"ToUnicode"));
 if(!f->cid){Value*encoding=resolve(e,get(e,font,"Encoding"));if(encoding&&encoding->type==DICT){Value*diff=resolve(e,get(e,encoding,"Differences"));if(diff&&diff->type==ARRAY){if(!f->unicode)f->unicode=allocate(e,65536*sizeof(unsigned));if(!f->unicode)return NULL;int code=0;for(Value*v=diff->child;v&&!e->failed;v=v->next){if(v->type==NUM)code=(int)v->number;else if(v->type==NAME){if(code<0||code>255){FAIL(e,"Codifica font PDF non valida");break;}/* ToUnicode takes precedence; otherwise glyph names use FreeType. */if(!f->unicode[code]){unsigned glyph=FT_Get_Name_Index(f->face,(char*)v->bytes);if(glyph)f->unicode[code]=0x80000000u|glyph;}code++;}}}}
 else if(encoding&&!named(encoding,"WinAnsiEncoding")&&!named(encoding,"StandardEncoding")){FAIL(e,"PDF: codifica font non ancora supportata");return NULL;}}
 return e->failed?NULL:f;
}
static double glyph_width(PdfEngine*e,Font*f,unsigned code,double fallback){
 if(!f->cid){Value*v=code>=(unsigned)f->first?at(e,f->widths,code-f->first):NULL;return numeric(e,v,fallback);}
 Value*v=f->widths&&f->widths->type==ARRAY?f->widths->child:NULL;
 while(v&&!e->failed){unsigned first=(unsigned)numeric(e,v,0);v=v->next;if(!v)break;Value*entry=resolve(e,v);v=v->next;if(entry&&entry->type==ARRAY){Value*width=entry->child;for(unsigned n=first;width; n++,width=width->next)if(n==code)return numeric(e,width,fallback);}else{unsigned last=(unsigned)numeric(e,entry,0);if(!v)break;double width=numeric(e,v,fallback);v=v->next;if(code>=first&&code<=last)return width;}}
 return numeric(e,get(e,f->metrics,"DW"),1000);
}
static void textshow(Render*r,Value*s){
 PdfEngine*e=r->e;Graphics*g=&r->g;if(!s||s->type!=STRING){FAIL(e,"Stringa testo PDF richiesta");return;}Font*f=font_get(r);if(!f)return;FT_Face face=f->face;Matrix m=concat(g->matrix,g->text);double size=fabs(g->font_size),sx=hypot(m.a,m.b)*g->hscale,sy=hypot(m.c,m.d);if(size<.01||sx<.0001||sy<.0001)return;if(!isfinite(size*sy)||size*sy>8192||!isfinite(sx/sy)){FAIL(e,"Dimensioni font PDF fuori limite");return;}if(fabs(m.b)>.01||fabs(m.c)>.01){FAIL(e,"PDF: testo ruotato/inclinato non ancora supportato");return;}unsigned pixel_size=(unsigned)clamp(size*sy,1,512);FT_Set_Pixel_Sizes(face,0,pixel_size);
 for(size_t i=0;i<s->length&&!e->failed;i++){
  unsigned code=s->bytes[i];if(f->cid){if(i+1==s->length){FAIL(e,"Testo CID PDF troncato");return;}code=(code<<8)|s->bytes[++i];}unsigned unicode=f->unicode&&f->unicode[code]?f->unicode[code]:unicode_byte(code),glyph;
  if(f->cid&&face!=e->fallback){glyph=code;if(f->gid){if((size_t)code*2+1>=f->gid_size){FAIL(e,"CIDToGIDMap PDF troncato");return;}glyph=(f->gid[code*2]<<8)|f->gid[code*2+1];}}
  else glyph=unicode&0x80000000u?unicode&0x7fffffffu:FT_Get_Char_Index(face,unicode);
  if(FT_Load_Glyph(face,glyph,FT_LOAD_DEFAULT)||FT_Render_Glyph(face->glyph,FT_RENDER_MODE_NORMAL)){FAIL(e,"Glyph PDF non leggibile");return;}FT_GlyphSlot slot=face->glyph;Point origin=transform(m,0,g->rise);double horizontal=sx/sy;if(!isfinite(origin.x)||!isfinite(origin.y)||fabs(origin.x)>1e7||fabs(origin.y)>1e7){FAIL(e,"Posizione testo PDF fuori limite");return;}if(horizontal>4096){FAIL(e,"Scala font PDF fuori limite");return;}double wide=ceil(slot->bitmap.width*horizontal);if(!isfinite(wide)||wide>4096){FAIL(e,"Glyph PDF troppo grande");return;}unsigned ww=(unsigned)wide;int base_x=(int)lround(origin.x+slot->bitmap_left*horizontal),base_y=(int)lround(origin.y-slot->bitmap_top);
  if(g->render_mode!=3){if(g->render_mode!=0){FAIL(e,"PDF: contorno/clipping testo non supportato");return;}for(unsigned y=0;y<slot->bitmap.rows;y++)for(unsigned x=0;x<ww;x++){unsigned source=(unsigned)(x/horizontal);if(source>=slot->bitmap.width)source=slot->bitmap.width-1;unsigned alpha=slot->bitmap.buffer[y*slot->bitmap.pitch+source];pixel(r,base_x+(int)x,base_y+(int)y,g->fill,alpha);}}
  double fallback=slot->advance.x/64./sy*1000./size;double advance=glyph_width(e,f,code,fallback)*size/1000.+g->charspace;if(code==32)advance+=g->wordspace;g->text=concat(g->text,(Matrix){1,0,0,1,advance*g->hscale,0});m=concat(g->matrix,g->text);
 }
}
static void contents(Render*,Value*);
static void execute(Render*,unsigned char*,size_t);
typedef struct{struct jpeg_error_mgr base;jmp_buf jump;}JpegError;
static void jpeg_fail(j_common_ptr c){JpegError*error=(JpegError*)c->err;longjmp(error->jump,1);}
static void image(Render*r,Object*o){
 PdfEngine*e=r->e;int width=(int)numeric(e,get(e,o->v,"Width"),0),height=(int)numeric(e,get(e,o->v,"Height"),0),bits=(int)numeric(e,get(e,o->v,"BitsPerComponent"),8);Value*cs=resolve(e,get(e,o->v,"ColorSpace"));int channels=named(cs,"DeviceRGB")?3:named(cs,"DeviceGray")?1:0;
 if(width<1||height<1||width>4096||height>4096||bits!=8||!channels||get(e,o->v,"SMask")||get(e,o->v,"Mask")||get(e,o->v,"Decode")){FAIL(e,"PDF: formato immagine o maschera non supportato");return;}size_t size;unsigned char*data=decode(e,o,&size,1);if(!data)return;
 Value*filter=resolve(e,get(e,o->v,"Filter"));if(filter&&filter->type==ARRAY)filter=resolve(e,filter->child);
 if(named(filter,"DCTDecode")){
  struct jpeg_decompress_struct jpeg={0};JpegError error;jpeg.err=jpeg_std_error(&error.base);error.base.error_exit=jpeg_fail;if(setjmp(error.jump)){jpeg_destroy_decompress(&jpeg);FAIL(e,"JPEG nel PDF danneggiato");return;}jpeg_create_decompress(&jpeg);jpeg_mem_src(&jpeg,data,size);jpeg_read_header(&jpeg,TRUE);jpeg.out_color_space=channels==3?JCS_RGB:JCS_GRAYSCALE;jpeg_start_decompress(&jpeg);
  if(jpeg.output_width!=(unsigned)width||jpeg.output_height!=(unsigned)height||jpeg.output_components!=channels){jpeg_destroy_decompress(&jpeg);FAIL(e,"Dimensioni JPEG PDF non valide");return;}size=(size_t)width*height*channels;data=allocate(e,size);if(!data){jpeg_destroy_decompress(&jpeg);return;}while(jpeg.output_scanline<jpeg.output_height){JSAMPROW row=data+(size_t)jpeg.output_scanline*width*channels;jpeg_read_scanlines(&jpeg,&row,1);}jpeg_finish_decompress(&jpeg);jpeg_destroy_decompress(&jpeg);
 }else{
  Value*params=resolve(e,get(e,o->v,"DecodeParms"));int predictor=(int)numeric(e,get(e,params,"Predictor"),1);
  if(predictor!=1){int columns=(int)numeric(e,get(e,params,"Columns"),1),colors=(int)numeric(e,get(e,params,"Colors"),1),bits_per_component=(int)numeric(e,get(e,params,"BitsPerComponent"),8);size_t row=(size_t)width*channels;
   if(columns!=width||colors!=channels||bits_per_component!=8||(predictor!=2&&(predictor<10||predictor>15))){FAIL(e,"PDF: predictor immagine non supportato");return;}
   size_t encoded_row=row+(predictor==2?0:1);if(size<encoded_row*(size_t)height){FAIL(e,"Immagine PDF con predictor troncata");return;}unsigned char*decoded=allocate(e,row*(size_t)height);if(!decoded)return;
   for(int y=0;y<height;y++){unsigned char*source=data+(size_t)y*encoded_row,*target=decoded+(size_t)y*row;int filter=predictor==2?1:*source++;if(filter>4){FAIL(e,"Predictor PNG nel PDF non valido");return;}for(size_t x=0;x<row;x++){int left=x>=(size_t)channels?target[x-channels]:0,up=y?(target-row)[x]:0,upper_left=y&&x>=(size_t)channels?(target-row)[x-channels]:0,value=source[x];if(filter==1)value+=left;if(filter==2)value+=up;if(filter==3)value+=(left+up)/2;if(filter==4){int p=left+up-upper_left,a=abs(p-left),b=abs(p-up),c=abs(p-upper_left);value+=a<=b&&a<=c?left:b<=c?up:upper_left;}target[x]=value;}}
   data=decoded;size=row*(size_t)height;
  }
 }

 if(size<(size_t)width*height*channels){FAIL(e,"Immagine PDF troncata");return;}Matrix m=r->g.matrix;double det=m.a*m.d-m.b*m.c;if(fabs(det)<1e-12)return;Point corners[4]={transform(m,0,0),transform(m,1,0),transform(m,1,1),transform(m,0,1)};double x0=r->w,y0=r->h,x1=0,y1=0;for(int i=0;i<4;i++){x0=fmin(x0,corners[i].x);x1=fmax(x1,corners[i].x);y0=fmin(y0,corners[i].y);y1=fmax(y1,corners[i].y);}for(int y=(int)clamp(floor(y0),0,r->h);y<(int)clamp(ceil(y1),0,r->h);y++)for(int x=(int)clamp(floor(x0),0,r->w);x<(int)clamp(ceil(x1),0,r->w);x++){double dx=x+.5-m.x,dy=y+.5-m.y,u=(m.d*dx-m.c*dy)/det,v=(-m.b*dx+m.a*dy)/det;if(u<0||u>=1||v<0||v>=1)continue;int ix=(int)(u*width),iy=(int)((1-v)*height);if(iy>=height)iy=height-1;unsigned char*p=data+((size_t)iy*width+ix)*channels;pixel(r,x,y,color(p[0]/255.,p[channels==3?1:0]/255.,p[channels==3?2:0]/255.),255);}
}
static void xobject(Render*r,Value*name){PdfEngine*e=r->e;Value*v=get(e,get(e,r->resources,"XObject"),(char*)name->bytes);Object*o=stream_object(e,v);if(!o)return;Value*sub=resolve(e,get(e,o->v,"Subtype"));if(named(sub,"Image")){image(r,o);return;}if(!named(sub,"Form")){FAIL(e,"PDF: XObject non supportato");return;}if(r->depth>=16){FAIL(e,"PDF: Form ricorsivo");return;}if(get(e,o->v,"Group")){FAIL(e,"PDF: trasparenza Form non ancora supportata");return;}Graphics saved=r->g;Value*resources=r->resources;Value*matrix=resolve(e,get(e,o->v,"Matrix"));if(matrix){Matrix m={numeric(e,at(e,matrix,0),1),numeric(e,at(e,matrix,1),0),numeric(e,at(e,matrix,2),0),numeric(e,at(e,matrix,3),1),numeric(e,at(e,matrix,4),0),numeric(e,at(e,matrix,5),0)};r->g.matrix=concat(r->g.matrix,m);}Value*rr=resolve(e,get(e,o->v,"Resources"));if(rr)r->resources=rr;size_t n;unsigned char*data=decode(e,o,&n,0);if(data){r->depth++;execute(r,data,n);r->depth--;}r->g=saved;r->resources=resources;}
static int operands(Render*r,Value**v,int count,int needed){if(count!=needed){FAIL(r->e,"Operandi PDF non validi");return 0;}for(int i=0;i<count;i++)if(v[i]->type!=NUM){FAIL(r->e,"Numero PDF richiesto");return 0;}return 1;}
static void execute(Render*r,unsigned char*data,size_t length){
 Parser p={data,data+length,r->e,0};Value*args[64];int count=0;
 while(p.p<p.end&&!r->e->failed){Value*v=parse(&p);if(!v)break;if(++r->operations>200000){FAIL(r->e,"PDF: pagina troppo complessa");break;}if(v->type!=WORD){if(count==64){FAIL(r->e,"PDF: troppi operandi");break;}args[count++]=v;continue;}
 char*op=(char*)v->bytes;double n[6]={0};for(int i=0;i<count&&i<6;i++)n[i]=args[i]->number;
#define OP(s) (!strcmp(op,s))
#define NUMBERS(k) operands(r,args,count,k)
 Graphics*g=&r->g;
 if(OP("q")){if(count||r->top==32){FAIL(r->e,"Stack grafico PDF fuori limite");break;}r->stack[r->top++]=*g;}
 else if(OP("Q")){if(count||!r->top){FAIL(r->e,"Stack grafico PDF non bilanciato");break;}*g=r->stack[--r->top];}
 else if(OP("cm")){if(NUMBERS(6))g->matrix=concat(g->matrix,(Matrix){n[0],n[1],n[2],n[3],n[4],n[5]});}
 else if(OP("rg")||OP("RG")){if(NUMBERS(3)){uint32_t c=color(n[0],n[1],n[2]);if(OP("rg"))g->fill=(c&0xffffff)|(g->fill&0xff000000);else g->stroke=(c&0xffffff)|(g->stroke&0xff000000);}}
 else if(OP("g")||OP("G")){if(NUMBERS(1)){uint32_t c=color(n[0],n[0],n[0]);if(OP("g"))g->fill=(c&0xffffff)|(g->fill&0xff000000);else g->stroke=(c&0xffffff)|(g->stroke&0xff000000);}}
 else if(OP("k")||OP("K")){if(NUMBERS(4)){uint32_t c=color(1-fmin(1,n[0]+n[3]),1-fmin(1,n[1]+n[3]),1-fmin(1,n[2]+n[3]));if(OP("k"))g->fill=(c&0xffffff)|(g->fill&0xff000000);else g->stroke=(c&0xffffff)|(g->stroke&0xff000000);}}
 else if(OP("cs")||OP("CS")){if(count!=1||args[0]->type!=NAME)FAIL(r->e,"Spazio colore PDF non valido");else{int space=named(args[0],"DeviceGray")?1:named(args[0],"DeviceRGB")?3:named(args[0],"DeviceCMYK")?4:0;if(!space)FAIL(r->e,"PDF: spazio colore non supportato");else if(OP("cs"))g->fill_space=space;else g->stroke_space=space;}}
 else if(OP("sc")||OP("scn")||OP("SC")||OP("SCN")){int fill=op[0]=='s',space=fill?g->fill_space:g->stroke_space;if(NUMBERS(space)){uint32_t c=space==1?color(n[0],n[0],n[0]):space==3?color(n[0],n[1],n[2]):color(1-fmin(1,n[0]+n[3]),1-fmin(1,n[1]+n[3]),1-fmin(1,n[2]+n[3]));if(fill)g->fill=(c&0xffffff)|(g->fill&0xff000000);else g->stroke=(c&0xffffff)|(g->stroke&0xff000000);}}
 else if(OP("gs")){if(count!=1||args[0]->type!=NAME)FAIL(r->e,"Stato grafico PDF non valido");else{Value*state=resolve(r->e,get(r->e,get(r->e,r->resources,"ExtGState"),(char*)args[0]->bytes));if(!state)FAIL(r->e,"Stato grafico PDF mancante");else{Value*mask=resolve(r->e,get(r->e,state,"SMask")),*blend=resolve(r->e,get(r->e,state,"BM"));if((mask&&!named(mask,"None"))||(blend&&!named(blend,"Normal")&&!named(blend,"Compatible")))FAIL(r->e,"PDF: maschera/fusione grafica non supportata");double fill=numeric(r->e,get(r->e,state,"ca"),1),stroke=numeric(r->e,get(r->e,state,"CA"),1);g->fill=(g->fill&0xffffff)|(unsigned)(clamp(fill,0,1)*255)<<24;g->stroke=(g->stroke&0xffffff)|(unsigned)(clamp(stroke,0,1)*255)<<24;g->linewidth=numeric(r->e,get(r->e,state,"LW"),g->linewidth);}}}
 else if(OP("w")){if(NUMBERS(1))g->linewidth=n[0];}
 else if(OP("m")||OP("l")){if(NUMBERS(2))point(r,n[0],n[1],OP("m"));}
 else if(OP("re")){if(NUMBERS(4)){point(r,n[0],n[1],1);point(r,n[0]+n[2],n[1],0);point(r,n[0]+n[2],n[1]+n[3],0);point(r,n[0],n[1]+n[3],0);closepath(r);}}
 else if(OP("h")){closepath(r);}
 else if(OP("c")){if(NUMBERS(6))curve(r,n[0],n[1],n[2],n[3],n[4],n[5]);}
 else if(OP("v")||OP("y")){if(NUMBERS(4)){if(!r->points)FAIL(r->e,"Curva PDF senza origine");else if(OP("y"))curve(r,n[0],n[1],n[2],n[3],n[2],n[3]);else{Matrix m=g->matrix;double det=m.a*m.d-m.b*m.c;if(fabs(det)>1e-12){Point last=r->path[r->points-1];double dx=last.x-m.x,dy=last.y-m.y;curve(r,(m.d*dx-m.c*dy)/det,(-m.b*dx+m.a*dy)/det,n[0],n[1],n[2],n[3]);}}}}
 else if(OP("f")||OP("F")||OP("f*")||OP("S")||OP("s")||OP("B")||OP("B*")||OP("b")||OP("b*")||OP("n")){
  if(OP("s")||OP("b")||OP("b*"))closepath(r);
  if(OP("f")||OP("F")||OP("f*")||OP("B")||OP("B*")||OP("b")||OP("b*"))fillpath(r,strchr(op,'*')!=NULL);
  if(OP("S")||OP("s")||OP("B")||OP("B*")||OP("b")||OP("b*"))stroke(r);
  if(r->clip){if(r->points!=5||!r->path[0].move||fabs(r->path[0].y-r->path[1].y)>.01||fabs(r->path[1].x-r->path[2].x)>.01||fabs(r->path[2].y-r->path[3].y)>.01||fabs(r->path[3].x-r->path[0].x)>.01){FAIL(r->e,"PDF: clipping non rettangolare non supportato");break;}double x0=fmin(r->path[0].x,r->path[2].x),x1=fmax(r->path[0].x,r->path[2].x),y0=fmin(r->path[0].y,r->path[2].y),y1=fmax(r->path[0].y,r->path[2].y);g->clipx0=(int)clamp(ceil(x0),g->clipx0,g->clipx1);g->clipx1=(int)clamp(ceil(x1),g->clipx0,g->clipx1);g->clipy0=(int)clamp(ceil(y0),g->clipy0,g->clipy1);g->clipy1=(int)clamp(ceil(y1),g->clipy0,g->clipy1);r->clip=0;}r->points=0;
 }
 else if(OP("W")||OP("W*")){r->clip=1;}
 else if(OP("BT")){g->text=g->line=identity();}
 else if(OP("ET")){}
 else if(OP("Tf")){if(count==2&&args[0]->type==NAME&&args[1]->type==NUM){g->font=get(r->e,get(r->e,r->resources,"Font"),(char*)args[0]->bytes);g->font_size=n[1];}else FAIL(r->e,"Font PDF non valido");}
 else if(OP("Tm")){if(NUMBERS(6))g->text=g->line=(Matrix){n[0],n[1],n[2],n[3],n[4],n[5]};}
 else if(OP("Td")||OP("TD")){if(NUMBERS(2)){if(OP("TD"))g->leading=-n[1];g->text=g->line=concat(g->line,(Matrix){1,0,0,1,n[0],n[1]});}}
 else if(OP("T*")){g->text=g->line=concat(g->line,(Matrix){1,0,0,1,0,-g->leading});}
 else if(OP("Tc")||OP("Tw")||OP("Tz")||OP("TL")||OP("Ts")||OP("Tr")){if(NUMBERS(1)){if(OP("Tc"))g->charspace=n[0];if(OP("Tw"))g->wordspace=n[0];if(OP("Tz"))g->hscale=n[0]/100.;if(OP("TL"))g->leading=n[0];if(OP("Ts"))g->rise=n[0];if(OP("Tr"))g->render_mode=(int)n[0];}}
 else if(OP("Tj")||OP("'")){if(count!=1)FAIL(r->e,"Testo PDF non valido");else{if(OP("'"))g->text=g->line=concat(g->line,(Matrix){1,0,0,1,0,-g->leading});textshow(r,args[0]);}}
 else if(OP("\"")){if(count==3&&args[0]->type==NUM&&args[1]->type==NUM&&args[2]->type==STRING){g->wordspace=n[0];g->charspace=n[1];g->text=g->line=concat(g->line,(Matrix){1,0,0,1,0,-g->leading});textshow(r,args[2]);}else FAIL(r->e,"Testo PDF non valido");}
 else if(OP("TJ")){if(count!=1||args[0]->type!=ARRAY)FAIL(r->e,"Testo PDF non valido");else for(Value*a=args[0]->child;a&&!r->e->failed;a=a->next){if(a->type==STRING)textshow(r,a);else if(a->type==NUM)g->text=concat(g->text,(Matrix){1,0,0,1,-a->number*g->font_size*g->hscale/1000.,0});else FAIL(r->e,"Array testo PDF non valido");}}
 else if(OP("Do")){if(count==1&&args[0]->type==NAME)xobject(r,args[0]);else FAIL(r->e,"XObject PDF non valido");}
 else if(OP("J")||OP("j")||OP("M")){if(!NUMBERS(1))break;/* Approximate joins/caps; documented rasterization limitation. */}
 else if(OP("d")){if(count!=2||args[0]->type!=ARRAY||args[0]->child||args[1]->type!=NUM)FAIL(r->e,"PDF: linee tratteggiate non ancora supportate");}
 else if(OP("BMC")||OP("BDC")||OP("EMC")||OP("MP")||OP("DP")){} /* Accessibility metadata; no painting. */
 else{char message[180];snprintf(message,sizeof(message),"PDF: operatore %.50s non ancora supportato",op);FAIL(r->e,message);}
 if(!isfinite(g->matrix.a)||!isfinite(g->matrix.b)||!isfinite(g->matrix.c)||!isfinite(g->matrix.d)||!isfinite(g->matrix.x)||!isfinite(g->matrix.y)||fabs(g->matrix.a)>1e7||fabs(g->matrix.b)>1e7||fabs(g->matrix.c)>1e7||fabs(g->matrix.d)>1e7||fabs(g->matrix.x)>1e7||fabs(g->matrix.y)>1e7)FAIL(r->e,"Trasformazione PDF fuori limite");
 count=0;
#undef OP
#undef NUMBERS
 }
 if(count&&!r->e->failed)FAIL(r->e,"Operatore PDF mancante");
}
static void contents(Render*r,Value*v){v=resolve(r->e,v);if(!v)return;if(v->type==ARRAY){for(Value*k=v->child;k&&!r->e->failed;k=k->next){Object*o=stream_object(r->e,k);size_t n;unsigned char*data=decode(r->e,o,&n,0);if(data)execute(r,data,n);}}else FAIL(r->e,"Contenuto PDF non valido");}
int pdf_engine_render(PdfEngine*e,int number,int zoom,int ox,int oy,unsigned width,unsigned height,uint32_t*pixels,int*maxx,int*maxy,char*error,size_t capacity){
 if(!e||number<0||number>=e->page_count||width<1||height<1||width>960||height>544||zoom<100||zoom>400||ox<0||oy<0||!pixels){snprintf(error,capacity,"Parametri rendering PDF non validi");return -1;}e->failed=0;e->message[0]=0;Block*mark=e->memory;size_t allocated=e->allocated;int nodes=e->nodes;Value*page=e->pages[number],*box=inherited(e,page,"CropBox");if(!box)box=inherited(e,page,"MediaBox");double x0=numeric(e,at(e,box,0),0),y0=numeric(e,at(e,box,1),0),x1=numeric(e,at(e,box,2),0),y1=numeric(e,at(e,box,3),0);double pw=x1-x0,ph=y1-y0;int rotation=(int)numeric(e,inherited(e,page,"Rotate"),0);if(rotation%360)FAIL(e,"PDF: pagine ruotate non ancora supportate");if(!isfinite(pw)||!isfinite(ph)||pw<=0||ph<=0||pw>100000||ph>100000)FAIL(e,"Dimensioni pagina PDF non valide");
 Render*r=calloc(1,sizeof(*r));if(!r)FAIL(e,"Memoria insufficiente");if(!e->failed){double scale=fmin(width/pw,height/ph)*zoom/100.,rw=pw*scale,rh=ph*scale;*maxx=rw>width?(int)ceil(rw-width):0;*maxy=rh>height?(int)ceil(rh-height):0;if(ox>*maxx)ox=*maxx;if(oy>*maxy)oy=*maxy;double tx=fmax(0,(width-rw)/2)-ox,ty=fmax(0,(height-rh)/2)-oy;
  r->e=e;r->pixels=pixels;r->w=width;r->h=height;r->resources=inherited(e,page,"Resources");r->g=(Graphics){.matrix={scale,0,0,-scale,tx-x0*scale,ty+y1*scale},.text=identity(),.line=identity(),.fill=0xff000000,.stroke=0xff000000,.linewidth=1,.font_size=12,.hscale=1,.fill_space=1,.stroke_space=1,.clipx1=width,.clipy1=height};
  for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++)pixels[y*width+x]=x>=tx&&x<tx+rw&&y>=ty&&y<ty+rh?0xffffffff:0xffe5e0db;
  r->g.clipx0=(int)clamp(ceil(tx),0,width);r->g.clipx1=(int)clamp(ceil(tx+rw),0,width);r->g.clipy0=(int)clamp(ceil(ty),0,height);r->g.clipy1=(int)clamp(ceil(ty+rh),0,height);
  Value*v=get(e,page,"Contents");Value*resolved=resolve(e,v);if(resolved&&resolved->type==ARRAY)contents(r,resolved);else if(v&&v->type==REF){Object*o=stream_object(e,v);size_t n;unsigned char*data=decode(e,o,&n,0);if(data)execute(r,data,n);}else if(v)contents(r,v);
  if(r->top&&!e->failed)FAIL(e,"Stack grafico PDF non bilanciato");
 }
 if(r)for(int i=0;i<r->font_count;i++)if(r->fonts[i].face&&r->fonts[i].face!=e->fallback)FT_Done_Face(r->fonts[i].face);
 free(r);int result=e->failed?-1:0;if(result)snprintf(error,capacity,"%s",e->message);while(e->memory!=mark){Block*b=e->memory;e->memory=b->next;free(b);}e->allocated=allocated;e->nodes=nodes;return result;
}
