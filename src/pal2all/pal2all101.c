// --------------------------------------------------
// Verion 1.0.1 (105)
// --------------------------------------------------
// Reads a 768-byte .PAL and emits into ./ChasmPalette/:
//   • Photoshop_<base>_transparent.act   (scaled 6→8-bit if needed)
//   • Photoshop_<base>_pink.act          (index 255 = #FC00C8)
//   • Quake_<base>.lmp                   (raw 768-byte pink palette)
//   • Jasc_<base>.pal                    (ASCII pink palette)
//   • Gimp_<base>.gpl                    (GIMP pink palette)
//   • PNG_<base>.png                     (16×16 preview of pink palette)
//   • Txt_<base>.txt                     (grid text of pink palette)
//   • Raw3_<base>.txt                    (3-byte hex per entry, pink palette)
//   • Raw4_<base>.txt                    (4-byte hex per entry, pink palette)
//   • ASE_<base>.ase                     (Adobe Swatch Exchange, pink palette)
//   • ACO_<base>.aco                     (Adobe Color Swatch v1, pink palette)
//   • Code_<base>.h                      (C header with pink palette array)
//   • Cube_<base>.cube                   (3D LUT size 16 mapping original palette)
// Usage:
//   x86_64-w64-mingw32-gcc -std=c99 -O2 -o pal2all.exe pal2all105.c
//   ./pal2all.exe input.pal
// --------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
  #include <direct.h>
  #define MKDIR(dir) _mkdir(dir)
#else
  #include <sys/stat.h>
  #define MKDIR(dir) mkdir(dir,0755)
#endif

// Portable big-endian converters
static uint16_t to_be16(uint16_t v) {
    return (uint16_t)((v << 8) | (v >> 8));
}
static uint32_t to_be32(uint32_t v) {
    return ((v >> 24) & 0x000000FF) |
           ((v >>  8) & 0x0000FF00) |
           ((v <<  8) & 0x00FF0000) |
           ((v << 24) & 0xFF000000);
}

// PNG / zlib helpers
static uint32_t crc_table[256];
static int crc_ready = 0;
static void make_crc_table(void) {
    for (int n = 0; n < 256; n++) {
        uint32_t c = (uint32_t)n;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc_table[n] = c;
    }
    crc_ready = 1;
}
static uint32_t update_crc(uint32_t crc, const uint8_t *buf, size_t len) {
    if (!crc_ready) make_crc_table();
    crc ^= 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++)
        crc = crc_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}
static uint32_t adler32(const uint8_t *data, size_t len) {
    const uint32_t MOD = 65521;
    uint32_t s1 = 1, s2 = 0;
    for (size_t i = 0; i < len; i++) {
        s1 = (s1 + data[i]) % MOD;
        s2 = (s2 + s1)      % MOD;
    }
    return (s2 << 16) | s1;
}
static void w32(FILE *f, uint32_t v) {
    fputc((v >> 24) & 0xFF, f);
    fputc((v >> 16) & 0xFF, f);
    fputc((v >>  8) & 0xFF, f);
    fputc((v      ) & 0xFF, f);
}
static void write_png_chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len) {
    w32(f, len);
    fwrite(type, 1, 4, f);
    if (len) fwrite(data, 1, len, f);
    uint8_t *buf = malloc(4 + len);
    memcpy(buf, type, 4);
    if (len) memcpy(buf + 4, data, len);
    uint32_t crc = update_crc(0, buf, 4 + len);
    free(buf);
    w32(f, crc);
}

// Write a 16×16 indexed PNG
static void write_png(const char *fn, uint8_t pal[256][3]) {
    const int W = 16, H = 16;
    size_t raw_len = H * (1 + W);
    uint8_t *raw = malloc(raw_len);
    for (int y = 0; y < H; y++) {
        raw[y*(W+1) + 0] = 0;
        for (int x = 0; x < W; x++)
            raw[y*(W+1) + 1 + x] = (uint8_t)(y*W + x);
    }
    uint8_t zhdr[2] = {0x78, 0x01};
    uint16_t blen = (uint16_t)raw_len;
    uint8_t dhdr[5] = {
        0x01,
        (uint8_t)( blen        &0xFF),
        (uint8_t)((blen >> 8) &0xFF),
        (uint8_t)(~blen        &0xFF),
        (uint8_t)((~blen >> 8)&0xFF)
    };
    uint32_t adl = adler32(raw, raw_len);
    size_t idat_len = 2 + 5 + raw_len + 4;
    uint8_t *idat = malloc(idat_len);
    size_t off = 0;
    memcpy(idat+off, zhdr, 2); off += 2;
    memcpy(idat+off, dhdr, 5); off += 5;
    memcpy(idat+off, raw, raw_len); off += raw_len;
    idat[off++] = (adl >> 24) & 0xFF;
    idat[off++] = (adl >> 16) & 0xFF;
    idat[off++] = (adl >>  8) & 0xFF;
    idat[off++] =  adl        & 0xFF;

    FILE *f = fopen(fn, "wb");
    if (!f) { perror("PNG fopen"); exit(1); }
    const uint8_t sig[8] = {137,80,78,71,13,10,26,10};
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13] = {0,0,0,W, 0,0,0,H, 8,3,0,0,0};
    write_png_chunk(f, "IHDR", ihdr, 13);
    write_png_chunk(f, "PLTE", &pal[0][0], 256*3);
    write_png_chunk(f, "IDAT", idat, (uint32_t)idat_len);
    write_png_chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw);
    free(idat);
}

// Write grid TXT
static void write_txt_grid(const char *fn, uint8_t pal[256][3]) {
    FILE *f = fopen(fn, "w");
    if (!f) { perror("txt"); exit(1); }
    fprintf(f,
        "ROWS 16\nCOLS 16\nWIDTH 16\nHEIGHT 16\n"
        "TEXTHEIGHT 0\nSPACING 1\n");
    for (int i = 0; i < 256; i++) {
        fprintf(f,
            "R: %03d, G: %03d, B: %03d\n",
            pal[i][0], pal[i][1], pal[i][2]);
    }
    fclose(f);
}

// Write raw hex TXT
static void write_raw_hex(const char *fn3, const char *fn4, uint8_t pal[256][3]) {
    FILE *f3 = fopen(fn3, "w");
    FILE *f4 = fopen(fn4, "w");
    if (!f3||!f4) { perror("raw hex"); exit(1); }
    for (int i = 0; i < 256; i++) {
        fprintf(f3, "0x%02X%02X%02X\n",   pal[i][0], pal[i][1], pal[i][2]);
        fprintf(f4, "0x%02X%02X%02X%02X\n", pal[i][0], pal[i][1], pal[i][2], 0xFF);
    }
    fclose(f3); fclose(f4);
}

// Corrected ACO writer (v1 only)
static void write_aco(const char *fn, uint8_t pal[256][3]) {
    FILE *f = fopen(fn, "wb");
    if (!f) { perror("aco fopen"); exit(1); }
    uint16_t ver = to_be16(1), count = to_be16(256);
    fwrite(&ver,2,1,f);
    fwrite(&count,2,1,f);
    for (int i = 0; i < 256; i++) {
        uint16_t space = to_be16(0);
        uint16_t r     = to_be16(pal[i][0]*257);
        uint16_t g     = to_be16(pal[i][1]*257);
        uint16_t b     = to_be16(pal[i][2]*257);
        uint16_t pad   = to_be16(0);
        fwrite(&space,2,1,f);
        fwrite(&r,    2,1,f);
        fwrite(&g,    2,1,f);
        fwrite(&b,    2,1,f);
        fwrite(&pad,  2,1,f);
    }
    fclose(f);
}

// Corrected ASE writer
static void write_ase(const char *fn, uint8_t pal[256][3]) {
    FILE *f = fopen(fn, "wb");
    if (!f) { perror("ase fopen"); exit(1); }
    fwrite("ASEF",1,4,f);
    uint16_t major = to_be16(1), minor = to_be16(0);
    fwrite(&major,2,1,f);
    fwrite(&minor,2,1,f);
    uint32_t nblocks = to_be32(256);
    fwrite(&nblocks,4,1,f);
    for (int i = 0; i < 256; i++) {
        uint16_t type = to_be16(1);
        char name[16];
        snprintf(name,sizeof(name),"Color%03d",i);
        uint16_t namelen = (uint16_t)(strlen(name)+1);
        uint16_t nl_be   = to_be16(namelen);
        uint32_t dlen    = 2 + namelen*2 + 4 + 12 + 2;
        uint32_t dlen_be = to_be32(dlen);
        fwrite(&type,2,1,f);
        fwrite(&dlen_be,4,1,f);
        fwrite(&nl_be,2,1,f);
        for(int j=0;j<namelen;j++){
            uint16_t uc = to_be16(j<(int)strlen(name)?name[j]:0);
            fwrite(&uc,2,1,f);
        }
        fwrite("RGB ",1,4,f);
        for(int c=0;c<3;c++){
            float fv = pal[i][c]/255.0f;
            uint32_t bits; memcpy(&bits,&fv,4);
            bits = to_be32(bits);
            fwrite(&bits,4,1,f);
        }
        uint16_t ct = to_be16(0);
        fwrite(&ct,2,1,f);
    }
    fclose(f);
}

// Utility: split basename
static void split_base(const char *path, char *out) {
    const char *fn = strrchr(path,'/');
#ifdef _WIN32
    const char *fn2 = strrchr(path,'\\');
    if(fn2 && (!fn||fn2>fn)) fn = fn2;
#endif
    if (fn) fn++; else fn = path;
    const char *dot = strrchr(fn,'.');
    size_t len = dot ? (size_t)(dot-fn) : strlen(fn);
    memcpy(out,fn,len);
    out[len] = '\0';
}
static void die(const char *msg) {
    fprintf(stderr,"%s\n",msg);
    exit(1);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr,"Usage: %s input.pal\n", argv[0]);
        return 1;
    }
    MKDIR("ChasmPalette");
    char base[256];
    split_base(argv[1], base);

    // Load palette
    FILE *fin = fopen(argv[1],"rb");
    if (!fin) die("opening .PAL");
    uint8_t pal_full[256][3];
    if (fread(pal_full,1,768,fin) != 768) die(".PAL must be 768 bytes");
    fclose(fin);

    // Detect & scale 6→8-bit
    uint8_t mx = 0;
    for(int i=0;i<256;i++) for(int c=0;c<3;c++)
        if(pal_full[i][c] > mx) mx = pal_full[i][c];
    if(mx <= 63)
        for(int i=0;i<256;i++) for(int c=0;c<3;c++)
            pal_full[i][c] <<= 2;

    // Pink override
    uint8_t pal_pink[256][3];
    memcpy(pal_pink, pal_full, 768);
    pal_pink[255][0] = 0xFC;
    pal_pink[255][1] = 0x00;
    pal_pink[255][2] = 0xC8;

    // 1) Photoshop ACTs
    char fn1[512], fn2[512];
    snprintf(fn1,sizeof(fn1),"ChasmPalette/Photoshop_%s_transparent.act",base);
    snprintf(fn2,sizeof(fn2),"ChasmPalette/Photoshop_%s_pink.act",       base);
    { FILE*f=fopen(fn1,"wb"); fwrite(pal_full,1,768,f); fclose(f); }
    { FILE*f=fopen(fn2,"wb"); fwrite(pal_pink,1,768,f); fclose(f); }

    // 2) Quake lump
    char fn_q[512]; snprintf(fn_q,sizeof(fn_q),"ChasmPalette/Quake_%s.lmp",base);
    { FILE*f=fopen(fn_q,"wb"); fwrite(pal_pink,1,768,f); fclose(f); }

    // 3) JASC-PAL
    char fn_j[512]; snprintf(fn_j,sizeof(fn_j),"ChasmPalette/Jasc_%s.pal",base);
    { FILE*f=fopen(fn_j,"w");
      fprintf(f,"JASC-PAL\n0100\n256\n");
      for(int i=0;i<256;i++)
        fprintf(f,"%d %d %d\n",
                pal_pink[i][0],pal_pink[i][1],pal_pink[i][2]);
      fclose(f);
    }

    // 4) GIMP GPL
    char fn_g[512]; snprintf(fn_g,sizeof(fn_g),"ChasmPalette/Gimp_%s.gpl",base);
    { FILE*f=fopen(fn_g,"w");
      fprintf(f,"GIMP Palette\nName: %s\nColumns: 16\n#\n",base);
      for(int i=0;i<256;i++)
        fprintf(f,"%3d %3d %3d\tColor%03d\n",
                pal_pink[i][0],pal_pink[i][1],pal_pink[i][2],i);
      fclose(f);
    }

    // 5) PNG preview
    char fn_p[512]; snprintf(fn_p,sizeof(fn_p),"ChasmPalette/PNG_%s.png",base);
    write_png(fn_p, pal_pink);

    // 6) Txt grid
    char fn_t[512]; snprintf(fn_t,sizeof(fn_t),"ChasmPalette/Txt_%s.txt",base);
    write_txt_grid(fn_t, pal_pink);

    // 7) Raw hex
    char fn_r3[512], fn_r4[512];
    snprintf(fn_r3,sizeof(fn_r3),"ChasmPalette/Raw3_%s.txt",base);
    snprintf(fn_r4,sizeof(fn_r4),"ChasmPalette/Raw4_%s.txt",base);
    write_raw_hex(fn_r3, fn_r4, pal_pink);

    // 8) ASE
    char fn_ase[512]; snprintf(fn_ase,sizeof(fn_ase),"ChasmPalette/ASE_%s.ase",base);
    write_ase(fn_ase, pal_pink);

    // 9) ACO
    char fn_aco[512]; snprintf(fn_aco,sizeof(fn_aco),"ChasmPalette/ACO_%s.aco",base);
    write_aco(fn_aco, pal_pink);

    // 10) C header
    char fn_c[512], guard[300];
    snprintf(guard,sizeof(guard),"%s_PALETTE_H",base);
    for(char *p=guard; *p; ++p) *p = isalpha(*p)? toupper(*p): '_';
    snprintf(fn_c,sizeof(fn_c),"ChasmPalette/Code_%s.h",base);
    { FILE*f=fopen(fn_c,"w");
      fprintf(f,
        "#ifndef %s\n#define %s\n\n#include <stdint.h>\n\n"
        "static const uint8_t %s_palette[256][3] = {\n",
        guard,guard,base);
      for(int i=0;i<256;i++){
        fprintf(f,"    { %3d, %3d, %3d }%s\n",
                pal_pink[i][0],pal_pink[i][1],pal_pink[i][2],
                i<255?",":"");
      }
      fprintf(f,"};\n\n#endif /* %s */\n",guard);
      fclose(f);
    }

    // 11) 3D LUT (.cube)
    const int N = 16;
    double pal_f[256][3];
    for(int i=0;i<256;i++){
      pal_f[i][0] = pal_full[i][0]/255.0;
      pal_f[i][1] = pal_full[i][1]/255.0;
      pal_f[i][2] = pal_full[i][2]/255.0;
    }
    char fn_cube[512];
    snprintf(fn_cube,sizeof(fn_cube),"ChasmPalette/Cube_%s.cube",base);
    FILE *fc = fopen(fn_cube,"w");
    if(!fc) die("creating .cube");
    fprintf(fc,"TITLE \"%s\"\nLUT_3D_SIZE %d\nDOMAIN_MIN 0.0 0.0 0.0\nDOMAIN_MAX 1.0 1.0 1.0\n",base,N);
    for(int bz=0;bz<N;bz++){
      double b=(double)bz/(N-1);
      for(int gy=0;gy<N;gy++){
        double g=(double)gy/(N-1);
        for(int rx=0;rx<N;rx++){
          double r=(double)rx/(N-1);
          int best=0; double bestd=1e9;
          for(int i=0;i<256;i++){
            double dr=pal_f[i][0]-r, dg=pal_f[i][1]-g, db=pal_f[i][2]-b;
            double d=dr*dr+dg*dg+db*db;
            if(d<bestd){ bestd=d; best=i; }
          }
          fprintf(fc,"%.6f %.6f %.6f\n",
                  pal_f[best][0],pal_f[best][1],pal_f[best][2]);
        }
      }
    }
    fclose(fc);

    printf("All exports written to ChasmPalette/ (6→8-bit scaled: %s)\n",
           mx<=63?"yes":"no");
    return 0;
}
