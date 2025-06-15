/*
 x86_64-w64-mingw32-gcc -O2 -o celtool.exe celtool104.c -lm

 Usage:
   celtool.exe -export <file.cel>
   celtool.exe -convert <file.png> [-diffusion | -pattern | -noise]
*/

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#pragma pack(push,1)
typedef struct {
    uint16_t type;
    uint16_t width;
    uint16_t height;
    uint16_t x;
    uint16_t y;
    uint8_t  depth;
    uint8_t  compress;
    uint32_t datasize;
    uint8_t  reserved[16];
} CelHeader;
#pragma pack(pop)

enum DitherMode { DITHER_NONE=0, DITHER_DIFFUSION, DITHER_PATTERN, DITHER_NOISE };

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s -export <file.cel>\n"
        "  %s -convert <file.png> [-diffusion|-pattern|-noise]\n",
        prog, prog);
}

/* scale 0..63 to 0..255 */
static void expand_palette(const uint8_t pal6[256][3], uint8_t pal8[256][3]) {
    for(int i = 0; i < 256; i++)
        for(int c = 0; c < 3; c++)
            pal8[i][c] = (pal6[i][c] * 255 + 31) / 63;
}

/* CEL -> PNG + alpha mask */
static int export_cel(const char *infile) {
    FILE *f = fopen(infile, "rb");
    if(!f) { perror("Error opening CEL file"); return 1; }
    CelHeader hdr;
    if(fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fprintf(stderr, "Error: failed to read header\n"); fclose(f); return 1;
    }
    if(hdr.type != 0x9119) {
        fprintf(stderr, "Error: not a valid Autodesk Animator CEL\n"); fclose(f); return 1;
    }
    uint16_t w = hdr.width, h = hdr.height;

    uint8_t pal6[256][3];
    if(fread(pal6, 3, 256, f) != 256) {
        fprintf(stderr, "Error: failed to read palette\n"); fclose(f); return 1;
    }
    fclose(f);

    uint8_t pal8[256][3];
    expand_palette(pal6, pal8);

    size_t npix = (size_t)w * h;
    uint8_t *indices = malloc(npix);
    if(!indices) { fprintf(stderr, "Error: memory allocation failure\n"); return 1; }

    f = fopen(infile, "rb");
    fseek(f, sizeof(CelHeader) + 256*3, SEEK_SET);
    if(fread(indices, 1, npix, f) != npix) {
        fprintf(stderr, "Error: failed to read image data\n");
        free(indices); fclose(f); return 1;
    }
    fclose(f);

    uint8_t *rgb  = malloc(npix * 3);
    uint8_t *rgba = malloc(npix * 4);
    if(!rgb || !rgba) {
        fprintf(stderr, "Error: memory allocation failure\n");
        free(indices); free(rgb); free(rgba); return 1;
    }

    for(size_t i = 0; i < npix; i++) {
        uint8_t idx = indices[i];
        uint8_t r = pal8[idx][0], g = pal8[idx][1], b = pal8[idx][2];
        rgb [3*i +0] = r;
        rgb [3*i +1] = g;
        rgb [3*i +2] = b;
        rgba[4*i +0] = r;
        rgba[4*i +1] = g;
        rgba[4*i +2] = b;
        rgba[4*i +3] = (idx == 255) ? 0 : 255;
    }

    char base[PATH_MAX];
    strncpy(base, infile, PATH_MAX);
    char *dot = strrchr(base, '.'); if(dot) *dot = '\0';

    char out_rgb[PATH_MAX], out_alpha[PATH_MAX];
    snprintf(out_rgb,   PATH_MAX, "%s.png",       base);
    snprintf(out_alpha, PATH_MAX, "%s_alpha.png", base);

    if(!stbi_write_png(out_rgb,     w, h, 3, rgb,  w*3) ||
       !stbi_write_png(out_alpha,   w, h, 4, rgba, w*4)) {
        fprintf(stderr, "Error: failed to write PNG files\n");
        free(indices); free(rgb); free(rgba); return 1;
    }

    printf("Export complete:\n");
    printf(" - %s  (size: %dx%d, RGB)\n", out_rgb,   w, h);
    printf(" - %s  (size: %dx%d, RGBA alpha)\n", out_alpha, w, h);

    free(indices);
    free(rgb);
    free(rgba);
    return 0;
}

/* PNG -> CEL (supports RGBA: alpha=0 -> index 255) */
static int convert_png(const char *infile, enum DitherMode mode) {
    int w, h, comp;
    uint8_t *img = stbi_load(infile, &w, &h, &comp, 4);
    if(!img) {
        fprintf(stderr, "Error loading PNG: %s\n", stbi_failure_reason());
        return 1;
    }
    size_t npix = (size_t)w * h;

    uint8_t actpal[256][3];
    FILE *pf = fopen("chasmpalette.act", "rb");
    if(!pf || fread(actpal, 1, 768, pf) != 768) {
        fprintf(stderr, "Error: failed to load chasmpalette.act\n");
        stbi_image_free(img);
        if(pf) fclose(pf);
        return 1;
    }
    fclose(pf);

    uint8_t *indices = malloc(npix);
    if(!indices) {
        fprintf(stderr, "Error: memory allocation failure\n");
        stbi_image_free(img);
        return 1;
    }

    /* Prepare dithering */
    float *err_r = NULL, *err_g = NULL, *err_b = NULL;
    float *next_r = NULL, *next_g = NULL, *next_b = NULL;
    if(mode == DITHER_DIFFUSION) {
        err_r  = calloc(w+2, sizeof(float));
        err_g  = calloc(w+2, sizeof(float));
        err_b  = calloc(w+2, sizeof(float));
        next_r = calloc(w+2, sizeof(float));
        next_g = calloc(w+2, sizeof(float));
        next_b = calloc(w+2, sizeof(float));
        if(!err_r||!err_g||!err_b||!next_r||!next_g||!next_b) {
            fprintf(stderr, "Error: memory allocation for dithering failed\n");
            free(indices); stbi_image_free(img);
            return 1;
        }
    }
    static const int bayer4[4][4] = {
        { 0,  8,  2, 10},
        {12,  4, 14,  6},
        { 3, 11,  1,  9},
        {15,  7, 13,  5}
    };
    int noise_amp = 16;
    if(mode == DITHER_NOISE) srand((unsigned)time(NULL));

    /* Quantize with dithering */
    for(int y = 0; y < h; y++) {
        if(mode == DITHER_DIFFUSION) {
            memset(next_r, 0, (w+2)*sizeof(float));
            memset(next_g, 0, (w+2)*sizeof(float));
            memset(next_b, 0, (w+2)*sizeof(float));
        }
        for(int x = 0; x < w; x++) {
            size_t idx = (size_t)y * w + x;
            uint8_t a = img[4*idx + 3];
            if(a == 0) {
                indices[idx] = 255;
                continue;
            }
            /* get base color */
            float r = img[4*idx + 0];
            float g = img[4*idx + 1];
            float b = img[4*idx + 2];
            /* apply dither */
            if(mode == DITHER_DIFFUSION) {
                r += err_r[x+1];
                g += err_g[x+1];
                b += err_b[x+1];
            } else if(mode == DITHER_PATTERN) {
                float t = (bayer4[y&3][x&3] - 7.5f) * 2.0f;
                r += t; g += t; b += t;
            } else if(mode == DITHER_NOISE) {
                int n = (rand() % (2*noise_amp + 1)) - noise_amp;
                r += n; g += n; b += n;
            }
            /* clamp [0..255] */
            r = r < 0 ? 0 : (r > 255 ? 255 : r);
            g = g < 0 ? 0 : (g > 255 ? 255 : g);
            b = b < 0 ? 0 : (b > 255 ? 255 : b);
            /* find best match */
            int best = 0;
            int min_err = INT_MAX;
            for(int j = 0; j < 256; j++) {
                int dr = (int)r - actpal[j][0];
                int dg = (int)g - actpal[j][1];
                int db = (int)b - actpal[j][2];
                int err = dr*dr + dg*dg + db*db;
                if(err < min_err) { min_err = err; best = j; if(err == 0) break; }
            }
            indices[idx] = (uint8_t)best;
            /* propagate error */
            if(mode == DITHER_DIFFUSION) {
                float er = r - actpal[best][0];
                float eg = g - actpal[best][1];
                float eb = b - actpal[best][2];
                err_r[x+2]     += er * 7.0f/16.0f;
                next_r[x]      += er * 3.0f/16.0f;
                next_r[x+1]    += er * 5.0f/16.0f;
                next_r[x+2]    += er * 1.0f/16.0f;
                err_g[x+2]     += eg * 7.0f/16.0f;
                next_g[x]      += eg * 3.0f/16.0f;
                next_g[x+1]    += eg * 5.0f/16.0f;
                next_g[x+2]    += eg * 1.0f/16.0f;
                err_b[x+2]     += eb * 7.0f/16.0f;
                next_b[x]      += eb * 3.0f/16.0f;
                next_b[x+1]    += eb * 5.0f/16.0f;
                next_b[x+2]    += eb * 1.0f/16.0f;
            }
        }
        if(mode == DITHER_DIFFUSION) {
            float *tmp;
            tmp = err_r; err_r = next_r; next_r = tmp;
            tmp = err_g; err_g = next_g; next_g = tmp;
            tmp = err_b; err_b = next_b; next_b = tmp;
        }
    }
    stbi_image_free(img);

    if(mode == DITHER_DIFFUSION) {
        free(err_r); free(err_g); free(err_b);
        free(next_r); free(next_g); free(next_b);
    }

    /* write CEL */
    CelHeader hdr = {0};
    hdr.type     = 0x9119;
    hdr.width    = (uint16_t)w;
    hdr.height   = (uint16_t)h;
    hdr.depth    = 8;
    hdr.compress = 0;
    hdr.datasize = (uint32_t)npix;

    uint8_t pal6_out[256][3];
    for(int j=0;j<256;j++) for(int c=0;c<3;c++)
        pal6_out[j][c] = (actpal[j][c] * 63 + 127) / 255;

    char base2[PATH_MAX]; strncpy(base2, infile, PATH_MAX);
    char *d2 = strrchr(base2, '.'); if(d2) *d2 = '\0';
    char suffix[16] = "";
    if(mode == DITHER_DIFFUSION) strcpy(suffix, "_diffusion");
    else if(mode == DITHER_PATTERN)   strcpy(suffix, "_pattern");
    else if(mode == DITHER_NOISE)     strcpy(suffix, "_noise");

    char out_cel[PATH_MAX];
    snprintf(out_cel, PATH_MAX, "%s%s.cel", base2, suffix);

    FILE *of = fopen(out_cel, "wb");
    if(!of) {
        perror("Error: opening output CEL");
        free(indices);
        return 1;
    }
    fwrite(&hdr,       sizeof(hdr),    1,    of);
    fwrite(pal6_out,   3,             256,  of);
    fwrite(indices,    1,             npix, of);
    fclose(of);

    printf("Conversion complete:\n");
    printf(" - %s  (size: %dx%d, dither: %s)\n",
        out_cel, w, h,
        mode==DITHER_DIFFUSION ? "diffusion" :
        mode==DITHER_PATTERN   ? "pattern"   :
        mode==DITHER_NOISE     ? "noise"     :
                                 "none");

    free(indices);
    return 0;
}

int main(int argc, char **argv) {
    if(argc < 3 || argc > 4) {
        print_usage(argv[0]);
        return 1;
    }
    if(strcmp(argv[1], "-export") == 0) {
        if(argc != 3) {
            print_usage(argv[0]); return 1;
        }
        return export_cel(argv[2]);
    } else if(strcmp(argv[1], "-convert") == 0) {
        enum DitherMode mode = DITHER_NONE;
        if(argc == 4) {
            if(strcmp(argv[3], "-diffusion") == 0) mode = DITHER_DIFFUSION;
            else if(strcmp(argv[3], "-pattern") == 0)  mode = DITHER_PATTERN;
            else if(strcmp(argv[3], "-noise") == 0)    mode = DITHER_NOISE;
            else { fprintf(stderr, "Unknown option: %s\n", argv[3]); print_usage(argv[0]); return 1; }
        }
        return convert_png(argv[2], mode);
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }
}
