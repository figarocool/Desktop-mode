#include "../wasm_app.h"
#define DM_WIDGETS_WASM
#include "../dm_widgets.h"

static int32_t count;

uint32_t dm_app_abi_version(void) { return DM_WASM_APP_ABI_VERSION; }
void dm_app_init(const char *argument) { (void)argument; count = 0; }
void dm_app_draw(int32_t width, int32_t height) {
    (void)width;
    (void)height;
    dm_host_text(24, 34, "Contatore", DM_WASM_COLOR_TEXT);
    dm_host_text(24, 70, "Premi il pulsante per aggiungere 1", DM_WASM_COLOR_TEXT);
    dmw_button((DmWidgetRect){24,86,190,38}, "Aggiungi 1", 0, 0);
    /* Format without libc so the example can be built freestanding. */
    char value[12];
    char reversed[12];
    uint32_t n = 0;
    uint32_t v = count < 0 ? (uint32_t)(-(count + 1)) + 1u : (uint32_t)count;
    do { reversed[n++] = (char)('0' + v % 10u); v /= 10u; } while (v && n < sizeof(reversed));
    uint32_t out = 0;
    if (count < 0) value[out++] = '-';
    while (n) value[out++] = reversed[--n];
    value[out] = 0;
    dm_host_text(24, 154, value, DM_WASM_COLOR_TEXT);
}
void dm_app_click(int32_t x, int32_t y, uint32_t buttons) {
    if (buttons && dmw_button_click((DmWidgetRect){24,86,190,38},x,y,0) && count < 2147483647)
        ++count;
}
void dm_app_menu(int32_t command){if(command==DM_WASM_MENU_RESET)count=0;}
void dm_app_close(void) { }
