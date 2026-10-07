#include "desktop_api.h"
#include "wasm_sandbox.h"
#include "vendor/wasm3/wasm3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define C(r,g,b,a) DM_COLOR(r,g,b,a)

/* Valid WebAssembly 1.0 modules: run() returns 42, or traps on a 64 KiB OOB load. */
static const uint8_t module_add[] = {
    0x00,0x61,0x73,0x6d,0x01,0x00,0x00,0x00,
    0x01,0x05,0x01,0x60,0x00,0x01,0x7f,
    0x03,0x02,0x01,0x00,
    0x05,0x04,0x01,0x01,0x01,0x01,
    0x07,0x07,0x01,0x03,0x72,0x75,0x6e,0x00,0x00,
    0x0a,0x09,0x01,0x07,0x00,0x41,0x06,0x41,0x24,0x6a,0x0b
};
static const uint8_t module_oob[] = {
    0x00,0x61,0x73,0x6d,0x01,0x00,0x00,0x00,
    0x01,0x05,0x01,0x60,0x00,0x01,0x7f,
    0x03,0x02,0x01,0x00,
    0x05,0x04,0x01,0x01,0x01,0x01,
    0x07,0x07,0x01,0x03,0x72,0x75,0x6e,0x00,0x00,
    0x0a,0x0b,0x01,0x09,0x00,0x41,0x80,0x80,0x04,0x28,0x02,0x00,0x0b
};

static int wasm_execute(const uint8_t *bytes,size_t size,int32_t *value,char *detail,size_t detail_size) {
    if (!bytes||!value||size>UINT32_MAX) return DM_WASM_INVALID;
    IM3Environment env=m3_NewEnvironment();
    if (!env) { snprintf(detail,detail_size,"wasm3: memoria insufficiente"); return DM_WASM_INVALID; }
    IM3Runtime runtime=m3_NewRuntime(env,64*1024,NULL);
    IM3Module module=NULL;
    int loaded=0;
    M3Result error=runtime?NULL:"wasm3: runtime non disponibile";
    if (!error) error=m3_ParseModule(env,&module,bytes,(uint32_t)size);
    if (!error) {
        error=m3_LoadModule(runtime,module);
        if (!error) loaded=1;
    }
    IM3Function function=NULL;
    if (!error) error=m3_FindFunction(&function,runtime,"run");
    if (!error) error=m3_CallV(function);
    if (!error) error=m3_GetResultsV(function,value);
    int result=DM_WASM_OK;
    if (error) {
        snprintf(detail,detail_size,"%.112s",error);
        result=(strstr(error,"trap")||strstr(error,"bounds")||strstr(error,"out of bounds"))?DM_WASM_TRAP:DM_WASM_INVALID;
    } else if (detail_size) detail[0]=0;
    if (module&&!loaded) m3_FreeModule(module);
    if (runtime) m3_FreeRuntime(runtime);
    m3_FreeEnvironment(env);
    return result;
}

int dm_wasm_run_builtin_tests(int *add_status,int32_t *value,int *trap_status) {
    if (!add_status||!value||!trap_status) return -1;
    char detail[128];
    *add_status=wasm_execute(module_add,sizeof(module_add),value,detail,sizeof(detail));
    int32_t ignored=0;
    *trap_status=wasm_execute(module_oob,sizeof(module_oob),&ignored,detail,sizeof(detail));
    return *add_status==DM_WASM_OK&&*value==42&&*trap_status==DM_WASM_TRAP?0:-1;
}

typedef struct { int add_status,trap_status,ran; int32_t value; } SandboxState;

static void draw_sandbox(DmWindow *w) {
    SandboxState *s=w->state;
    dm_text(w->x+24,w->y+62,"Sandbox WebAssembly - wasm3 0.5.0",C(24,38,55,255));
    dm_text(w->x+24,w->y+94,"Runtime completo; memoria istanza di test: 64 KiB.",C(50,67,84,255));
    dm_rect(w->x+24,w->y+118,245,42,C(184,211,238,255));
    dm_text(w->x+42,w->y+145,"Esegui test sandbox",C(24,38,55,255));
    if (s->ran) {
        char line[128];
        snprintf(line,sizeof(line),"Modulo A: stato %d, risultato %d",s->add_status,s->value);
        dm_text(w->x+24,w->y+188,line,C(24,38,55,255));
        snprintf(line,sizeof(line),"Modulo B (lettura OOB): stato %d",s->trap_status);
        dm_text(w->x+24,w->y+220,line,C(24,38,55,255));
        dm_text(w->x+24,w->y+252,s->add_status==DM_WASM_OK&&s->value==42&&s->trap_status==DM_WASM_TRAP?"PASS: trap catturato, shell ancora attiva.":"FAIL: controllo sandbox non superato.",C(20,112,58,255));
    }
    dm_text(w->x+24,w->y+w->h-34,"Il runtime wasm3 interpreta moduli WebAssembly 1.0.",C(78,91,105,255));
}

static void click_sandbox(DmWindow *w,int x,int y) {
    if (x<24||x>=269||y<118||y>=160) return;
    SandboxState *s=w->state;
    dm_wasm_run_builtin_tests(&s->add_status,&s->value,&s->trap_status);
    s->ran=1;
}

const DmApp dm_wasm_sandbox_app={DM_API_VERSION,"wasmsandbox","Sandbox WebAssembly - test",sizeof(SandboxState),0,draw_sandbox,click_sandbox,0,0,0,"",0};
