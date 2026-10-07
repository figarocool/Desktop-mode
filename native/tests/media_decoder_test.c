#define _POSIX_C_SOURCE 200809L
#include "../app_runtime.h"
#include <webp/encode.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vita2d.h>
#include <psp2/io/stat.h>

static unsigned live_textures;
DmWindow *dm_current_window;
void dm_ui_transform_rect(int *x, int *y, int *w, int *h) {
    (void)x; (void)y; (void)w; (void)h;
}

int sceIoOpen(const char *path, int flags, int mode) { (void)mode; return open(path, flags); }
int sceIoRead(int fd, void *buffer, unsigned size) { return (int)read(fd, buffer, size); }
int sceIoClose(int fd) { return close(fd); }
int sceIoGetstat(const char *path, SceIoStat *result) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    result->st_size = st.st_size;
    result->st_mode = (unsigned)st.st_mode;
    return 0;
}

vita2d_texture *vita2d_create_empty_texture(unsigned width, unsigned height) {
    vita2d_texture *texture = calloc(1, sizeof(*texture));
    if (!texture) return NULL;
    texture->pixels = calloc((size_t)width * height, 4);
    if (!texture->pixels) { free(texture); return NULL; }
    texture->width = width;
    texture->height = height;
    live_textures++;
    return texture;
}
vita2d_texture *vita2d_load_PNG_file(const char *path) { (void)path; return NULL; }
vita2d_texture *vita2d_load_JPEG_file(const char *path) { (void)path; return NULL; }
unsigned vita2d_texture_get_width(vita2d_texture *t) { return t->width; }
unsigned vita2d_texture_get_height(vita2d_texture *t) { return t->height; }
unsigned vita2d_texture_get_stride(vita2d_texture *t) { return t->width * 4; }
unsigned char *vita2d_texture_get_datap(vita2d_texture *t) { return t->pixels; }
void vita2d_free_texture(vita2d_texture *t) {
    if (!t) return;
    free(t->pixels);
    free(t);
    live_textures--;
}
void vita2d_draw_texture_part_scale(vita2d_texture *t, int x, int y, unsigned sx, unsigned sy,
                                    unsigned sw, unsigned sh, float scale_x, float scale_y) {
    (void)t; (void)x; (void)y; (void)sx; (void)sy; (void)sw; (void)sh; (void)scale_x; (void)scale_y;
}
void vita2d_draw_texture_scale(vita2d_texture *t, int x, int y, float scale_x, float scale_y) {
    (void)t; (void)x; (void)y; (void)scale_x; (void)scale_y;
}
void dm_memory_image(void *image, uint64_t bytes) { (void)image; (void)bytes; }
void dm_memory_image_free(void *image) { (void)image; }
int dm_fs_write(const char *path, const void *data, size_t size, int exclusive) {
    (void)path; (void)data; (void)size; (void)exclusive; return -1;
}

int main(void) {
    char root[] = "/tmp/desktop-mode-image-XXXXXX";
    assert(mkdtemp(root));
    char path[256];
    snprintf(path, sizeof(path), "%s/sample.webp", root);
    uint8_t source[4 * 3 * 4];
    for (unsigned y = 0; y < 3; y++) for (unsigned x = 0; x < 4; x++) {
        size_t i = ((size_t)y * 4 + x) * 4;
        source[i] = (uint8_t)(10 + x * 30);
        source[i + 1] = (uint8_t)(20 + y * 40);
        source[i + 2] = (uint8_t)(200 - x * 20);
        source[i + 3] = (uint8_t)(50 + y * 60);
    }
    uint8_t *encoded = NULL;
    size_t encoded_size = WebPEncodeLosslessRGBA(source, 4, 3, 4 * 4, &encoded);
    assert(encoded_size > 12 && encoded);
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(encoded, 1, encoded_size, file) == encoded_size);
    fclose(file);
    WebPFree(encoded);

    vita2d_texture *texture = dm_image_load(path);
    assert(texture && live_textures == 1);
    unsigned width = 0, height = 0;
    dm_image_size(texture, &width, &height);
    assert(width == 4 && height == 3);
    assert(memcmp(texture->pixels, source, sizeof(source)) == 0);
    dm_image_free(texture);
    assert(live_textures == 0);

    file = fopen(path, "wb");
    assert(file && fputs("RIFFbrokenWEBP", file) >= 0);
    fclose(file);
    assert(dm_image_load(path) == NULL && live_textures == 0);
    unlink(path);
    rmdir(root);
    puts("PASS: native WebP decode, RGBA pixels, malformed input rejection and texture cleanup");
    return 0;
}
