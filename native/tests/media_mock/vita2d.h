#ifndef DESKTOP_MODE_TEST_VITA2D_H
#define DESKTOP_MODE_TEST_VITA2D_H
#include <stdint.h>
typedef struct vita2d_texture {
    unsigned width, height;
    uint8_t *pixels;
} vita2d_texture;
vita2d_texture *vita2d_create_empty_texture(unsigned width, unsigned height);
vita2d_texture *vita2d_load_PNG_file(const char *path);
vita2d_texture *vita2d_load_JPEG_file(const char *path);
unsigned vita2d_texture_get_width(vita2d_texture *texture);
unsigned vita2d_texture_get_height(vita2d_texture *texture);
unsigned vita2d_texture_get_stride(vita2d_texture *texture);
unsigned char *vita2d_texture_get_datap(vita2d_texture *texture);
void vita2d_free_texture(vita2d_texture *texture);
void vita2d_draw_texture_part_scale(vita2d_texture *, int, int, unsigned, unsigned, unsigned, unsigned, float, float);
void vita2d_draw_texture_scale(vita2d_texture *, int, int, float, float);
#endif
