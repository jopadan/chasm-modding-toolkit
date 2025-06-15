// x86_64-w64-mingw32-gcc -std=c99 -O2 -o carextractpng.exe carextractpng.c
// carextractpng.exe test.car


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define TEXTURE_WIDTH           64
#define TEXTURE_OFFSET_BYTES    0x486Au
#define TEXTURE_OFFSET          0x486Cu

// Transparent color (#040404)
static const uint8_t TRANSPARENT_R = 0x04;
static const uint8_t TRANSPARENT_G = 0x04;
static const uint8_t TRANSPARENT_B = 0x04;

static void write_png_rgba(const char *filename, int w, int h, uint8_t *rgba_data)
{
    int stride = w * 4;
    if (!stbi_write_png(filename, w, h, 4, rgba_data, stride)) {
        fprintf(stderr, "Failed to write PNG \"%s\"\n", filename);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input.car>\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *car_file = argv[1];
    const char *act_file = "chasmpalette.act";

    // --- load .CAR file into memory ---
    FILE *cf = fopen(car_file, "rb");
    if (!cf) { perror("Opening .CAR"); return EXIT_FAILURE; }
    fseek(cf, 0, SEEK_END);
    long file_size = ftell(cf);
    rewind(cf);
    uint8_t *car_data = malloc(file_size);
    if (!car_data || fread(car_data, 1, file_size, cf) != file_size) {
        fprintf(stderr, "Error reading %s\n", car_file);
        return EXIT_FAILURE;
    }
    fclose(cf);

    // --- load chasmpalette.act (768 bytes: R, G, B × 256) ---
    FILE *af = fopen(act_file, "rb");
    if (!af) { perror("Opening chasmpalette.act"); return EXIT_FAILURE; }
    uint8_t palette[256][3];
    if (fread(palette, 1, 256*3, af) != 256*3) {
        fprintf(stderr, "Error reading %s\n", act_file);
        return EXIT_FAILURE;
    }
    fclose(af);

    // --- extract first 64×N texture ---
    uint16_t tex_size;
    memcpy(&tex_size, car_data + TEXTURE_OFFSET_BYTES, sizeof(tex_size));
    int tex_h = tex_size / TEXTURE_WIDTH;
    uint8_t *tex_idx = car_data + TEXTURE_OFFSET;

    size_t tex_pixels = (size_t)TEXTURE_WIDTH * tex_h;
    uint8_t *tex_rgba = malloc(tex_pixels * 4);
    if (!tex_rgba) {
        fprintf(stderr, "Out of memory\n");
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < tex_pixels; i++) {
        uint8_t idx = tex_idx[i];
        uint8_t r = palette[idx][0];
        uint8_t g = palette[idx][1];
        uint8_t b = palette[idx][2];
        size_t base = i * 4;
        tex_rgba[base + 0] = r;
        tex_rgba[base + 1] = g;
        tex_rgba[base + 2] = b;
        tex_rgba[base + 3] = (r == TRANSPARENT_R && g == TRANSPARENT_G && b == TRANSPARENT_B) ? 0 : 255;
    }

    char out_file[512];
    snprintf(out_file, sizeof(out_file), "%s.texture.png", car_file);
    write_png_rgba(out_file, TEXTURE_WIDTH, tex_h, tex_rgba);

    free(tex_rgba);
    free(car_data);

    return EXIT_SUCCESS;
}
