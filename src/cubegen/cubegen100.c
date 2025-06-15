// cubegen.c v1.0.0 (2023-10-01)
// --------------------------------------------------
// 3D LUT Cube Generator
// Loads a 256-entry palette from:
//   • Photoshop .act / Quake .lmp   (768-byte binary RGB)
//   • JASC .pal                     (ASCII “JASC-PAL”)
//   • GIMP .gpl                     (ASCII GIMP Palette)
//   • 16×16 indexed .png            (via stb_image.h)
// Automatically writes <inputbasename>_<format>.cube.
//
// Usage:
//   cubegen.exe -help
//   cubegen.exe input.act
//   cubegen.exe input.pal
//   cubegen.exe input.gpl
//   cubegen.exe input.png
//
// Build (MinGW/WSL):
// x86_64-w64-mingw32-gcc -std=c99 -O2 -DSTB_IMAGE_IMPLEMENTATION cubegen100.c -lm -o cubegen.exe cubegen.res
//
// Place stb_image.h alongside cubegen.c.
// --------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include "stb_image.h"

static uint8_t pal[256][3];

static void scale6to8() {
    uint8_t mx = 0;
    for (int i = 0; i < 256; i++)
        for (int c = 0; c < 3; c++)
            if (pal[i][c] > mx) mx = pal[i][c];
    if (mx <= 63)
        for (int i = 0; i < 256; i++)
            for (int c = 0; c < 3; c++)
                pal[i][c] <<= 2;
}

// load 768-byte binary (.act, .lmp, raw .PAL)
static int load_binary(const char *fn) {
    FILE *f = fopen(fn, "rb");
    if (!f) return 0;
    if (fread(pal, 1, 768, f) != 768) { fclose(f); return 0; }
    fclose(f);
    scale6to8();
    return 1;
}

// load JASC-PAL ASCII
static int load_jasc(const char *fn) {
    FILE *f = fopen(fn, "r");
    if (!f) return 0;
    char buf[128];
    if (!fgets(buf, sizeof(buf), f) || strncmp(buf, "JASC-PAL", 8)) { fclose(f); return 0; }
    fgets(buf, sizeof(buf), f);
    fgets(buf, sizeof(buf), f);
    for (int i = 0; i < 256; i++) {
        int r, g, b;
        if (fscanf(f, "%d %d %d", &r, &g, &b) != 3) { fclose(f); return 0; }
        pal[i][0] = r; pal[i][1] = g; pal[i][2] = b;
    }
    fclose(f);
    scale6to8();
    return 1;
}

// load GIMP .gpl ASCII
static int load_gpl(const char *fn) {
    FILE *f = fopen(fn, "r");
    if (!f) return 0;
    char buf[128];
    while (fgets(buf, sizeof(buf), f) && buf[0] != '#');
    for (int i = 0; i < 256; i++) {
        int r, g, b;
        if (fscanf(f, "%d %d %d", &r, &g, &b) != 3) { fclose(f); return 0; }
        pal[i][0] = r; pal[i][1] = g; pal[i][2] = b;
        fgets(buf, sizeof(buf), f);
    }
    fclose(f);
    scale6to8();
    return 1;
}

// load 16×16 indexed PNG (extract palette from pixels)
static int load_png16(const char *fn) {
    int w, h, comp;
    unsigned char *img = stbi_load(fn, &w, &h, &comp, 3);
    if (!img) return 0;
    if (w != 16 || h != 16) { stbi_image_free(img); return 0; }
    for (int i = 0; i < 256; i++) {
        pal[i][0] = img[i*3+0];
        pal[i][1] = img[i*3+1];
        pal[i][2] = img[i*3+2];
    }
    stbi_image_free(img);
    scale6to8();
    return 1;
}

// find nearest palette entry for (r,g,b) ∈ [0..1]
static int find_best(double r, double g, double b) {
    int best = 0; double bd = 1e9;
    for (int i = 0; i < 256; i++) {
        double dr = pal[i][0]/255.0 - r;
        double dg = pal[i][1]/255.0 - g;
        double db = pal[i][2]/255.0 - b;
        double d = dr*dr + dg*dg + db*db;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

// write .cube file
static int write_cube(const char *fn) {
    FILE *f = fopen(fn, "w");
    if (!f) return 0;
    const int N = 16;
    fprintf(f, "TITLE \"3D LUT\"\nLUT_3D_SIZE %d\n", N);
    fprintf(f, "DOMAIN_MIN 0.0 0.0 0.0\nDOMAIN_MAX 1.0 1.0 1.0\n");
    for (int b = 0; b < N; b++) {
        double db = b/(double)(N-1);
        for (int g = 0; g < N; g++) {
            double dg = g/(double)(N-1);
            for (int r = 0; r < N; r++) {
                double dr = r/(double)(N-1);
                int idx = find_best(dr, dg, db);
                fprintf(f, "%.6f %.6f %.6f\n",
                        pal[idx][0]/255.0,
                        pal[idx][1]/255.0,
                        pal[idx][2]/255.0);
            }
        }
    }
    fclose(f);
    return 1;
}

static void print_help(const char *pname) {
    printf("Usage:\n"
           "  %s -help\n"
           "  %s <input.{act,lmp,pal,gpl,png}>\n\n",
           pname, pname);
    printf("Supported inputs:\n"
           "  .act, .lmp   768-byte binary\n"
           "  .pal         JASC-PAL ASCII or raw 768-byte .PAL\n"
           "  .gpl         GIMP palette ASCII\n"
           "  .png         16w x 16h indexed preview PNG\n"
    "IMPORTANT: For best results use palette without PINK transparency!!!\n");
    printf("Output: <inputbasename>_<format>.cube\n");
}

static char *make_output_name(const char *in) {
    const char *dot = strrchr(in, '.');
    size_t base_len = dot ? (size_t)(dot - in) : strlen(in);
    const char *ext = dot ? dot + 1 : "";
    char *out = malloc(base_len + 1 + strlen(ext) + 6);
    memcpy(out, in, base_len);
    out[base_len] = '_';
    strcpy(out + base_len + 1, ext);
    strcpy(out + base_len + 1 + strlen(ext), ".cube");
    return out;
}

int main(int argc, char **argv) {
    if (argc != 2 || !strcmp(argv[1], "-help") || !strcmp(argv[1], "--help")) {
        print_help(argv[0]);
        return 0;
    }
    const char *in = argv[1];
    const char *dot = strrchr(in, '.');
    int ok = 0;
    if (dot) {
        if (!strcasecmp(dot, ".pal")) {
            // check header to distinguish JASC vs raw binary
            char header[16] = {0};
            FILE *f = fopen(in, "r");
            if (f) {
                fgets(header, sizeof(header), f);
                fclose(f);
            }
            if (!strncmp(header, "JASC-PAL", 8))
                ok = load_jasc(in);
            else
                ok = load_binary(in);
        }
        else if (!strcasecmp(dot, ".act") || !strcasecmp(dot, ".lmp"))
            ok = load_binary(in);
        else if (!strcasecmp(dot, ".gpl"))
            ok = load_gpl(in);
        else if (!strcasecmp(dot, ".png"))
            ok = load_png16(in);
    }
    if (!ok) {
        fprintf(stderr, "Error: failed to load palette '%s'\n", in);
        return 1;
    }
    char *out = make_output_name(in);
    if (!write_cube(out)) {
        fprintf(stderr, "Error: failed to write cube '%s'\n", out);
        free(out);
        return 1;
    }
    printf("3D LUT written to %s\n", out);
    free(out);
    return 0;
}
