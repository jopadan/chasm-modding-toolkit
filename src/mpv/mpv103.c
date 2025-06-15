// MPV.c V1.0.3 by SMR9000 (v103)
// --------------------------------------------------
// Multi Palette Viewer (MPV)
// Loads various palette formats (excluding ASE) and displays as a fixed-size 16×16 grid window.
// Window size is locked at 512×512 and cannot be resized.
//
// Usage:
//   mpv.exe -help
//   mpv.exe <palette_file>
//
// Supported formats:
//   .act, .lmp    Photoshop / Quake 768-byte binary
//   .pal          JASC-PAL ASCII
//   .gpl          GIMP palette ASCII
//   .txt          grid ("ROWS 16…" + "R: xxx…") or raw-hex (0xRRGGBB / 0xRRGGBBAA)
//   .raw3/.raw4  (as .txt containing 0xRRGGBB or 0xRRGGBBAA lines)
//   .aco          Adobe Color Swatch v1
//   .h            C header with static const uint8_t …[256][3]
//   .png/.pcx     16×16 indexed preview via stb_image.h
//
// Build (MinGW/WSL):
// x86_64-w64-mingw32-gcc -std=c99 -O2 -DSTB_IMAGE_IMPLEMENTATION MPV102.c     -Iinclude -Llib -lfreeglut -lopengl32 -lglu32 -lm -o mpv.exe multipaletteviewer.res
//
// Place stb_image.h alongside MPV.c.
// --------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <GL/freeglut.h>
#include "stb_image.h"   // STB_IMAGE_IMPLEMENTATION defined at compile time

#ifndef _GNU_SOURCE
char *strcasestr(const char *hay, const char *needle) {
    if (!*needle) return (char*)hay;
    for (; *hay; hay++) {
        const char *h=hay, *n=needle;
        while (*h && *n && tolower((unsigned char)*h)==tolower((unsigned char)*n)) { h++; n++; }
        if (!*n) return (char*)hay;
        hay++;
    }
    return NULL;
}
#endif

static uint8_t palette[256][3];

// scale 6→8 bit if needed
static void scale6to8(){
    uint8_t mx=0;
    for(int i=0;i<256;i++)for(int c=0;c<3;c++) if(palette[i][c]>mx) mx=palette[i][c];
    if(mx<=63) for(int i=0;i<256;i++)for(int c=0;c<3;c++) palette[i][c]<<=2;
}

// ── 768-byte binary loader (.act, .lmp, raw .PAL) ─────────
static int load_binary(const char *fn){
    FILE *f=fopen(fn,"rb"); if(!f) return 0;
    if(fread(palette,1,768,f)!=768){ fclose(f); return 0; }
    fclose(f); scale6to8(); return 1;
}

// ── JASC .pal ASCII ────────────────────────────────────────
static int load_jasc(const char *fn){
    FILE *f=fopen(fn,"r"); if(!f) return 0;
    char buf[128];
    if(!fgets(buf,sizeof(buf),f)||strncmp(buf,"JASC-PAL",8)){ fclose(f); return 0; }
    fgets(buf,sizeof(buf),f); fgets(buf,sizeof(buf),f);
    for(int i=0;i<256;i++){
        int r,g,b;
        if(fscanf(f,"%d %d %d",&r,&g,&b)!=3){ fclose(f); return 0; }
        palette[i][0]=r; palette[i][1]=g; palette[i][2]=b;
    }
    fclose(f); scale6to8(); return 1;
}

// ── GIMP .gpl ASCII ────────────────────────────────────────
static int load_gpl(const char *fn){
    FILE *f=fopen(fn,"r"); if(!f) return 0;
    char buf[128];
    while(fgets(buf,sizeof(buf),f)&&buf[0]!='#');
    for(int i=0;i<256;i++){
        int r,g,b;
        if(fscanf(f,"%d %d %d",&r,&g,&b)!=3){ fclose(f); return 0; }
        palette[i][0]=r; palette[i][1]=g; palette[i][2]=b;
        fgets(buf,sizeof(buf),f);
    }
    fclose(f); scale6to8(); return 1;
}

// ── Txt grid .txt ───────────────────────────────────────────
static int load_txtgrid(const char *fn){
    FILE *f=fopen(fn,"r"); if(!f) return 0;
    char buf[128];
    for(int i=0;i<6;i++) if(!fgets(buf,sizeof(buf),f)){ fclose(f); return 0; }
    for(int i=0;i<256;i++){
        int r,g,b;
        if(!fgets(buf,sizeof(buf),f)||sscanf(buf,"R: %d, G: %d, B: %d",&r,&g,&b)!=3){
            fclose(f); return 0;
        }
        palette[i][0]=r; palette[i][1]=g; palette[i][2]=b;
    }
    fclose(f); scale6to8(); return 1;
}

// ── Raw3 .txt (0xRRGGBB) ────────────────────────────────────
static int load_raw3(const char *fn){
    FILE *f=fopen(fn,"r"); if(!f) return 0;
    char buf[128];
    for(int i=0;i<256;i++){
        if(!fgets(buf,sizeof(buf),f)){ fclose(f); return 0; }
        unsigned r,g,b;
        if(sscanf(buf,"0x%2x%2x%2x",&r,&g,&b)!=3){ fclose(f); return 0; }
        palette[i][0]=r; palette[i][1]=g; palette[i][2]=b;
    }
    fclose(f); scale6to8(); return 1;
}

// ── Raw4 .txt (0xRRGGBBAA) ──────────────────────────────────
static int load_raw4(const char *fn){
    FILE *f=fopen(fn,"r"); if(!f) return 0;
    char buf[128];
    for(int i=0;i<256;i++){
        if(!fgets(buf,sizeof(buf),f)){ fclose(f); return 0; }
        unsigned r,g,b,a;
        if(sscanf(buf,"0x%2x%2x%2x%2x",&r,&g,&b,&a)!=4){ fclose(f); return 0; }
        palette[i][0]=r; palette[i][1]=g; palette[i][2]=b;
    }
    fclose(f); scale6to8(); return 1;
}

// ── ACO v1 ───────────────────────────────────────────────────
static int load_aco(const char *fn){
    FILE *f=fopen(fn,"rb"); if(!f) return 0;
    uint16_t ver,cnt; fread(&ver,2,1,f); fread(&cnt,2,1,f);
    ver=(ver<<8)|(ver>>8); cnt=(cnt<<8)|(cnt>>8);
    if(ver!=1||cnt<256){ fclose(f); return 0; }
    for(int i=0;i<256;i++){
        uint16_t sp,r,g,b,p;
        fread(&sp,2,1,f); fread(&r,2,1,f);
        fread(&g,2,1,f); fread(&b,2,1,f);
        fread(&p,2,1,f);
        r=(r<<8)|(r>>8); g=(g<<8)|(g>>8); b=(b<<8)|(b>>8);
        palette[i][0]=r>>8; palette[i][1]=g>>8; palette[i][2]=b>>8;
    }
    fclose(f); scale6to8(); return 1;
}

// ── C header .h ─────────────────────────────────────────────
static int load_c_header(const char *fn){
    FILE *f=fopen(fn,"r"); if(!f) return 0;
    fseek(f,0,SEEK_END); size_t sz=ftell(f); rewind(f);
    char *buf=malloc(sz+1); fread(buf,1,sz,f); buf[sz]=0; fclose(f);
    char *p=strchr(buf,'{'); if(!p){ free(buf); return 0; }
    p++;
    int vals[768],cnt=0;
    while(cnt<768 && *p){
        while(*p && !isdigit((unsigned char)*p)) p++;
        if(!*p) break;
        vals[cnt++]=strtol(p,&p,10);
    }
    free(buf); if(cnt<768) return 0;
    for(int i=0;i<256;i++){
        palette[i][0]=vals[i*3+0];
        palette[i][1]=vals[i*3+1];
        palette[i][2]=vals[i*3+2];
    }
    scale6to8(); return 1;
}

// ── PNG/PCX via stb_image (16×16 preview) ───────────────────
static int load_image16(const char *fn){
    int w,h,c; unsigned char *img=stbi_load(fn,&w,&h,&c,3);
    if(!img||w!=16||h!=16){ if(img) stbi_image_free(img); return 0; }
    for(int i=0;i<256;i++){
        palette[i][0]=img[i*3+0];
        palette[i][1]=img[i*3+1];
        palette[i][2]=img[i*3+2];
    }
    stbi_image_free(img); scale6to8(); return 1;
}

// ── dispatch loader ─────────────────────────────────────────
static int load_palette(const char *fn){
    const char *ext=strrchr(fn,'.'); if(!ext) return 0;
    if(!strcasecmp(ext,".act")||!strcasecmp(ext,".lmp")) return load_binary(fn);
    if(!strcasecmp(ext,".pal")){
        // try JASC first, then raw .PAL binary
        if(load_jasc(fn)) return 1;
        return load_binary(fn);
    }
    if(!strcasecmp(ext,".gpl"))    return load_gpl(fn);
    if(!strcasecmp(ext,".txt")){
        FILE *f=fopen(fn,"r"); if(!f) return 0;
        char buf[64]; fgets(buf,sizeof(buf),f); fclose(f);
        if(!strncmp(buf,"ROWS",4))             return load_txtgrid(fn);
        if(strcasestr(buf,"0x")&&strchr(buf,'A')) return load_raw4(fn);
        if(strcasestr(buf,"0x"))               return load_raw3(fn);
        return 0;
    }
    if(!strcasecmp(ext,".aco"))    return load_aco(fn);
    if(!strcasecmp(ext,".h"))      return load_c_header(fn);
    if(!strcasecmp(ext,".png")||!strcasecmp(ext,".pcx")) return load_image16(fn);
    return 0;
}

// print help
static void print_help(const char *p){
    printf("Usage:\n  %s -help\n  %s <palette_file>\n\n",p,p);
    printf("Supported formats:\n"
           "  .act, .lmp    768-byte binary\n"
           "  .pal          JASC-PAL ASCII or raw 768-byte .PAL\n"
           "  .gpl          GIMP ASCII\n"
           "  .txt          grid or raw-hex\n"
           "  .aco          Adobe Color Swatch v1\n"
           "  .h            C header [256][3]\n"
           "  .png, .pcx    16×16 preview\n");
}

// prevent resizing
static void reshape(int w,int h){
    glutReshapeWindow(512,512);
}

// display grid
static void display(void){
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_QUADS);
    for(int i=0;i<256;i++){
        int x=i%16, y=i/16;
        float sx=x/16.0f, sy=1.0f-(y+1)/16.0f;
        float w=1.0f/16.0f, h=1.0f/16.0f;
        glColor3ub(palette[i][0],palette[i][1],palette[i][2]);
        glVertex2f(sx,   sy);
        glVertex2f(sx+w, sy);
        glVertex2f(sx+w, sy+h);
        glVertex2f(sx,   sy+h);
    }
    glEnd();
    glutSwapBuffers();
}

int main(int argc,char **argv){
    if(argc!=2||(strcmp(argv[1],"-help")==0)||(strcmp(argv[1],"--help")==0)){
        print_help(argv[0]);
        return 0;
    }
    if(!load_palette(argv[1])){
        fprintf(stderr,"Error: unsupported or invalid palette format\n\n");
        print_help(argv[0]);
        return 1;
    }
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA);
    glutInitWindowSize(512,512);
    glutCreateWindow("MPV - Multi Palette Viewer v1.0.3 by SMR9000");
    glutReshapeFunc(reshape);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0,1,0,1,-1,1);
    glMatrixMode(GL_MODELVIEW);
    glutDisplayFunc(display);
    glutMainLoop();
    return 0;
}
