#include "../wasm_sandbox.h"
#include <assert.h>

void dm_text(int x,int y,const char*s,uint32_t color){(void)x;(void)y;(void)s;(void)color;}
void dm_rect(int x,int y,int w,int h,uint32_t color){(void)x;(void)y;(void)w;(void)h;(void)color;}

int main(void) {
    int add_status=-99,trap_status=-99; int32_t value=-1;
    assert(dm_wasm_run_builtin_tests(&add_status,&value,&trap_status)==0);
    assert(add_status==DM_WASM_OK&&value==42&&trap_status==DM_WASM_TRAP);
    return 0;
}
