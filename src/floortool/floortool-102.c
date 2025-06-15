// floors_viewer.c v128 (1.0.2)
// x86_64-w64-mingw32-gcc floors120-FINAL.c -o floortool.exe -I. -I./GL -L./lib -lfreeglut -lopengl32 -lm floortool.res
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#ifdef _WIN32
  #include <windows.h>
  #include <direct.h>
  #define MKDIR(dir) _mkdir(dir)
#else
  #include <sys/stat.h>
  #define MKDIR(dir) mkdir(dir,0755)
#endif

#include <GL/freeglut.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

//―――― Config ―――――――――――――――――――――――――

#define MAX_FLOORS       64
#define TEX_SIZE         64
#define HEADER_SIZE      64
#define UNKNOWN_BYTES    64
#define UNDO_DEPTH       100

#define MIN_WINDOW_W     512
#define MIN_WINDOW_H     512
#define DEFAULT_WINDOW_W 768
#define DEFAULT_WINDOW_H 512

//―――― Enums & Globals ―――――――――――――――――――――――――

enum { MODE_GRID=0, MODE_TILE=1, MODE_MIPMAP=2 };
typedef struct { int x,y; } Point;

typedef enum {
  DITHER_NONE = 0,
  DITHER_FS,
  DITHER_SL,
  DITHER_BAYER4,
  DITHER_BAYER8,
  DITHER_NOISE,
  DITHER_ATKINSON,
  DITHER_COUNT
} DitherMode;

typedef struct {
    unsigned char rgba[TEX_SIZE*TEX_SIZE*4];
    unsigned char idx[TEX_SIZE*TEX_SIZE];
    int tile_index;
} UndoState;

static const char *ditherNames[DITHER_COUNT] = {
  "None (Default)",
  "Floyd–Steinberg",
  "Sierra Lite",
  "Bayer 4×4 (Recommended) ",
  "Bayer 8×8",
  "Noise",
  "Atkinson"
};

static DitherMode importDitherMode = DITHER_NONE;
static UndoState undoStack[UNDO_DEPTH];
static int undoTop = -1;

static const char *g_filename;
static unsigned char *fileBuf = NULL;
static long fileSize = 0;

static unsigned char floorIdx[MAX_FLOORS][TEX_SIZE*TEX_SIZE];
static unsigned char *floorRGBA[MAX_FLOORS],
                     *floorMip1Data[MAX_FLOORS],
                     *floorMip2Data[MAX_FLOORS],
                     *floorMip3Data[MAX_FLOORS];

static GLuint texBase[MAX_FLOORS],
              texMip1[MAX_FLOORS],
              texMip2[MAX_FLOORS],
              texMip3[MAX_FLOORS];

static unsigned char palette[256][3];
static int tileFlags[MAX_FLOORS] = {0};

static Point sel = {0,0};
static int mode = MODE_GRID;
static float zoomLevel = 1.0f, zoomStep = 0.1f;
static int bgIndex, defaultBgIndex;
static bool showUI = true, dirty = false;

static int windowW = DEFAULT_WINDOW_W,
           windowH = DEFAULT_WINDOW_H;
static int viewportX, viewportY, viewportW, viewportH;

//―――― Prototypes ―――――――――――――――――――――――――

void saveGridPNG(void);
void saveTilesPNG(void);
void copyTileToClipboard(void);
void pasteTileFromClipboard(void);
void importGridPNG(void);
void importTilesFromManifest(void);
void importSelectedTile(void);
void saveFileXX(void);
void setClearColorFromPalette(void);
void drawText(const char *s,int x,int y);
void renderOverlay(void);
void computeViewport(int w,int h);
unsigned char* generateMip(unsigned char *src,int sw,int sh,int *dw,int *dh);
void loadPalette(const char *path);
void loadFloors(const char *path);
void initGL(void);
void flipTileHorizontal(void);
void flipTileVertical(void);
void rotateTile90(void);
void updateTileTextures(int ti);
void pushUndoState(int ti);
void undoLastAction(void);

// Build base filename without extension or path
static void buildBase(char *out){
    const char *fn = strrchr(g_filename,'/') ? strrchr(g_filename,'/')+1 : g_filename;
    fn = strrchr(fn,'\\') ? strrchr(fn,'\\')+1 : fn;
    int j=0;
    for(int i=0; fn[i] && j<255; i++){
        if(fn[i] != '.') out[j++] = fn[i];
    }
    out[j] = '\0';
}

//―――― Dither Algorithms ―――――――――――――――――――――

static const unsigned char bayer4[4][4] = {
  {  0,  8,  2, 10}, { 12,  4, 14,  6},
  {  3, 11,  1,  9}, { 15,  7, 13,  5}
};

static const unsigned char bayer8[8][8] = {
  {  0,32, 8,40, 2,34,10,42},{48,16,56,24,50,18,58,26},
  {12,44, 4,36,14,46, 6,38},{60,28,52,20,62,30,54,22},
  { 3,35,11,43, 1,33, 9,41},{51,19,59,27,49,17,57,25},
  {15,47, 7,39,13,45, 5,37},{63,31,55,23,61,29,53,21}
};

void applyFSDither(unsigned char *rgba,int W,int H){
    int size=W*H;
    float *fr=malloc(size*sizeof(float)),
          *fg=malloc(size*sizeof(float)),
          *fb=malloc(size*sizeof(float));
    for(int i=0;i<size;i++){
        fr[i]=rgba[4*i+0];
        fg[i]=rgba[4*i+1];
        fb[i]=rgba[4*i+2];
    }
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        int i=y*W+x;
        float oldr=fr[i], oldg=fg[i], oldb=fb[i];
        int best=0; float bd=1e9f;
        for(int c=0;c<256;c++){
            float dr=oldr-palette[c][0],
                  dg=oldg-palette[c][1],
                  db=oldb-palette[c][2],
                  d=dr*dr+dg*dg+db*db;
            if(d<bd){bd=d;best=c;}
        }
        float nr=palette[best][0], ng=palette[best][1], nb=palette[best][2];
        rgba[4*i+0]=(unsigned char)nr;
        rgba[4*i+1]=(unsigned char)ng;
        rgba[4*i+2]=(unsigned char)nb;
        float er=oldr-nr, eg=oldg-ng, eb=oldb-nb;
        #define DIFFUSE(dx,dy,f) \
          if(x+dx>=0&&x+dx<W&&y+dy>=0&&y+dy<H){ \
            fr[(y+dy)*W + (x+dx)] += er*f; \
            fg[(y+dy)*W + (x+dx)] += eg*f; \
            fb[(y+dy)*W + (x+dx)] += eb*f; }
        DIFFUSE(1, 0, 7.0f/16.0f);
        DIFFUSE(-1,1, 3.0f/16.0f);
        DIFFUSE(0, 1, 5.0f/16.0f);
        DIFFUSE(1, 1, 1.0f/16.0f);
        #undef DIFFUSE
    }
    free(fr); free(fg); free(fb);
}

void applySLDither(unsigned char *rgba,int W,int H){
    int size=W*H;
    float *fr=malloc(size*sizeof(float)),
          *fg=malloc(size*sizeof(float)),
          *fb=malloc(size*sizeof(float));
    for(int i=0;i<size;i++){
        fr[i]=rgba[4*i+0];
        fg[i]=rgba[4*i+1];
        fb[i]=rgba[4*i+2];
    }
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        int i=y*W+x;
        float oldr=fr[i], oldg=fg[i], oldb=fb[i];
        int best=0; float bd=1e9f;
        for(int c=0;c<256;c++){
            float dr=oldr-palette[c][0],
                  dg=oldg-palette[c][1],
                  db=oldb-palette[c][2],
                  d=dr*dr+dg*dg+db*db;
            if(d<bd){bd=d;best=c;}
        }
        float nr=palette[best][0], ng=palette[best][1], nb=palette[best][2];
        rgba[4*i+0]=(unsigned char)nr;
        rgba[4*i+1]=(unsigned char)ng;
        rgba[4*i+2]=(unsigned char)nb;
        float er=oldr-nr, eg=oldg-ng, eb=oldb-nb;
        #define SL(dx,dy,f) \
          if(x+dx>=0&&x+dx<W&&y+dy>=0&&y+dy<H){ \
            fr[(y+dy)*W + (x+dx)] += er*f; \
            fg[(y+dy)*W + (x+dx)] += eg*f; \
            fb[(y+dy)*W + (x+dx)] += eb*f; }
        SL(1,0, 2.0f/4.0f);
        SL(-1,1,1.0f/4.0f);
        SL(0,1, 1.0f/4.0f);
        #undef SL
    }
    free(fr); free(fg); free(fb);
}

void applyBayer4(unsigned char *rgba,int W,int H){
    for(int y=0;y<H;y++)for(int x=0;x<W;x++){
        int i=y*W+x, t=bayer4[y%4][x%4]-8;
        for(int k=0;k<3;k++){
            int v=rgba[4*i+k]+t;
            rgba[4*i+k] = v<0?0:(v>255?255:v);
        }
    }
}

void applyBayer8(unsigned char *rgba,int W,int H){
    for(int y=0;y<H;y++)for(int x=0;x<W;x++){
        int i=y*W+x, t=bayer8[y%8][x%8]-32;
        for(int k=0;k<3;k++){
            int v=rgba[4*i+k]+t;
            rgba[4*i+k] = v<0?0:(v>255?255:v);
        }
    }
}

void applyNoiseDither(unsigned char *rgba,int W,int H){
    for(int i=0;i<W*H;i++){
        int n=(rand()%17)-8;
        for(int k=0;k<3;k++){
            int v=rgba[4*i+k]+n;
            rgba[4*i+k] = v<0?0:(v>255?255:v);
        }
    }
}

void applyAtkinsonDither(unsigned char *rgba, int W, int H) {
    int size = W * H;
    float *fr = malloc(size * sizeof(float)),
          *fg = malloc(size * sizeof(float)),
          *fb = malloc(size * sizeof(float));

    for (int i = 0; i < size; i++) {
        fr[i] = rgba[4*i + 0];
        fg[i] = rgba[4*i + 1];
        fb[i] = rgba[4*i + 2];
    }

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int i = y * W + x;
            float oldr = fr[i], oldg = fg[i], oldb = fb[i];
            int best = 0; float bd = 1e9f;

            for (int c = 0; c < 256; c++) {
                float dr = oldr - palette[c][0],
                      dg = oldg - palette[c][1],
                      db = oldb - palette[c][2];
                float d = dr*dr + dg*dg + db*db;
                if (d < bd) { bd = d; best = c; }
            }

            float nr = palette[best][0], ng = palette[best][1], nb = palette[best][2];
            rgba[4*i + 0] = (unsigned char)nr;
            rgba[4*i + 1] = (unsigned char)ng;
            rgba[4*i + 2] = (unsigned char)nb;

            float er = oldr - nr, eg = oldg - ng, eb = oldb - nb;

            #define ATK(dx, dy, f) \
                if (x + dx >= 0 && x + dx < W && y + dy >= 0 && y + dy < H) { \
                    int j = (y + dy) * W + (x + dx); \
                    fr[j] += er * f; \
                    fg[j] += eg * f; \
                    fb[j] += eb * f; \
                }

            ATK(1, 0, 1/8.0f);
            ATK(2, 0, 1/8.0f);
            ATK(-1, 1, 1/8.0f);
            ATK(0, 1, 1/8.0f);
            ATK(1, 1, 1/8.0f);
            ATK(0, 2, 1/8.0f);
            #undef ATK
        }
    }

    free(fr); free(fg); free(fb);
}

void applyDither(unsigned char *rgba,int W,int H){
    switch(importDitherMode){
      case DITHER_FS:      applyFSDither(rgba,W,H); break;
      case DITHER_SL:     applySLDither(rgba,W,H); break;
      case DITHER_BAYER4: applyBayer4(rgba,W,H);   break;
      case DITHER_BAYER8: applyBayer8(rgba,W,H);   break;
      case DITHER_NOISE:  applyNoiseDither(rgba,W,H); break;
      case DITHER_ATKINSON: applyAtkinsonDither(rgba,W,H); break;
      default: break;
    }
}

//―――― Undo System ―――――――――――――――――――――――――

void pushUndoState(int ti) {
    undoTop = (undoTop + 1) % UNDO_DEPTH;
    memcpy(undoStack[undoTop].rgba, floorRGBA[ti], TEX_SIZE*TEX_SIZE*4);
    memcpy(undoStack[undoTop].idx, floorIdx[ti], TEX_SIZE*TEX_SIZE);
    undoStack[undoTop].tile_index = ti;
}

void undoLastAction(void) {
    if(undoTop < 0) return;
    
    int ti = undoStack[undoTop].tile_index;
    memcpy(floorRGBA[ti], undoStack[undoTop].rgba, TEX_SIZE*TEX_SIZE*4);
    memcpy(floorIdx[ti], undoStack[undoTop].idx, TEX_SIZE*TEX_SIZE);
    
    int w1,h1,w2,h2;
    free(floorMip1Data[ti]); 
    free(floorMip2Data[ti]); 
    free(floorMip3Data[ti]);
    floorMip1Data[ti] = generateMip(floorRGBA[ti],64,64,&w1,&h1);
    floorMip2Data[ti] = generateMip(floorMip1Data[ti],w1,h1,&w2,&h2);
    floorMip3Data[ti] = generateMip(floorMip2Data[ti],w2,h2,&w2,&h2);
    
    updateTileTextures(ti);
    undoTop--;
    dirty = true;
    glutPostRedisplay();
}

//―――― Mipmap Generator ―――――――――――――――――――――――――

unsigned char* generateMip(unsigned char *src,int sw,int sh,int *dw,int *dh){
    *dw=sw/2; *dh=sh/2;
    int w2=*dw, h2=*dh;
    unsigned char *dst=malloc(w2*h2*4);
    for(int y=0;y<h2;y++)for(int x=0;x<w2;x++){
        int r0=y*2,c0=x*2;
        int i00=(r0*sw+c0)*4, i10=(r0*sw+c0+1)*4;
        int i01=((r0+1)*sw+c0)*4,i11=((r0+1)*sw+c0+1)*4;
        for(int k=0;k<3;k++){
            int v=src[i00+k]+src[i10+k]+src[i01+k]+src[i11+k];
            dst[(y*w2+x)*4+k]=v/4;
        }
        dst[(y*w2+x)*4+3]=255;
    }
    return dst;
}

//―――― Texture Updates ―――――――――――――――――――――――――

void updateTileTextures(int ti) {
    glBindTexture(GL_TEXTURE_2D, texBase[ti]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, floorRGBA[ti]);
    
    glBindTexture(GL_TEXTURE_2D, texMip1[ti]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, floorMip1Data[ti]);
    
    glBindTexture(GL_TEXTURE_2D, texMip2[ti]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, floorMip2Data[ti]);
    
    glBindTexture(GL_TEXTURE_2D, texMip3[ti]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, floorMip3Data[ti]);
}

//―――― Tile Manipulation ―――――――――――――――――――――――――

void flipTileHorizontal(void){
    int ti = sel.y*8 + sel.x;
    pushUndoState(ti);
    
    unsigned char newRGBA[64*64*4];
    unsigned char newIdx[64*64];
    
    for(int y=0;y<64;y++){
        for(int x=0;x<64;x++){
            int src = y*64 + x;
            int dst = y*64 + (63 - x);
            memcpy(&newRGBA[dst*4], &floorRGBA[ti][src*4], 4);
            newIdx[dst] = floorIdx[ti][src];
        }
    }
    
    memcpy(floorRGBA[ti], newRGBA, 64*64*4);
    memcpy(floorIdx[ti], newIdx, 64*64);
    
    int w1,h1,w2,h2;
    free(floorMip1Data[ti]); free(floorMip2Data[ti]); free(floorMip3Data[ti]);
    floorMip1Data[ti] = generateMip(floorRGBA[ti],64,64,&w1,&h1);
    floorMip2Data[ti] = generateMip(floorMip1Data[ti],w1,h1,&w2,&h2);
    floorMip3Data[ti] = generateMip(floorMip2Data[ti],w2,h2,&w2,&h2);
    
    updateTileTextures(ti);
    dirty = true;
    glutPostRedisplay();
}

void flipTileVertical(void){
    int ti = sel.y*8 + sel.x;
    pushUndoState(ti);
    
    unsigned char newRGBA[64*64*4];
    unsigned char newIdx[64*64];
    
    for(int y=0;y<64;y++){
        for(int x=0;x<64;x++){
            int src = y*64 + x;
            int dst = (63 - y)*64 + x;
            memcpy(&newRGBA[dst*4], &floorRGBA[ti][src*4], 4);
            newIdx[dst] = floorIdx[ti][src];
        }
    }
    
    memcpy(floorRGBA[ti], newRGBA, 64*64*4);
    memcpy(floorIdx[ti], newIdx, 64*64);
    
    int w1,h1,w2,h2;
    free(floorMip1Data[ti]); free(floorMip2Data[ti]); free(floorMip3Data[ti]);
    floorMip1Data[ti] = generateMip(floorRGBA[ti],64,64,&w1,&h1);
    floorMip2Data[ti] = generateMip(floorMip1Data[ti],w1,h1,&w2,&h2);
    floorMip3Data[ti] = generateMip(floorMip2Data[ti],w2,h2,&w2,&h2);
    
    updateTileTextures(ti);
    dirty = true;
    glutPostRedisplay();
}

void rotateTile90(void){
    int ti = sel.y*8 + sel.x;
    pushUndoState(ti);
    
    unsigned char newRGBA[64*64*4];
    unsigned char newIdx[64*64];
    
    for(int y=0;y<64;y++){
        for(int x=0;x<64;x++){
            int src = y*64 + x;
            int dst = x*64 + (63 - y);
            memcpy(&newRGBA[dst*4], &floorRGBA[ti][src*4], 4);
            newIdx[dst] = floorIdx[ti][src];
        }
    }
    
    memcpy(floorRGBA[ti], newRGBA, 64*64*4);
    memcpy(floorIdx[ti], newIdx, 64*64);
    
    int w1,h1,w2,h2;
    free(floorMip1Data[ti]); free(floorMip2Data[ti]); free(floorMip3Data[ti]);
    floorMip1Data[ti] = generateMip(floorRGBA[ti],64,64,&w1,&h1);
    floorMip2Data[ti] = generateMip(floorMip1Data[ti],w1,h1,&w2,&h2);
    floorMip3Data[ti] = generateMip(floorMip2Data[ti],w2,h2,&w2,&h2);
    
    updateTileTextures(ti);
    dirty = true;
    glutPostRedisplay();
}

//―――― File I/O and Imports ―――――――――――――――――――――――――

void loadPalette(const char *path){
    FILE *f=fopen(path,"rb"); if(!f){fprintf(stderr,"pal\n");exit(1);}
    for(int i=0;i<256;i++) fread(palette[i],1,3,f);
    fclose(f);
    defaultBgIndex=0;
    for(int i=0;i<256;i++){
        if(palette[i][0]==0x48&&palette[i][1]==0x58&&palette[i][2]==0x58){
            defaultBgIndex=i; break;
        }
    }
    bgIndex=defaultBgIndex;
}

void loadFloors(const char *path){
    FILE *f=fopen(path,"rb"); if(!f){fprintf(stderr,"open %s\n",path);exit(1);}
    fseek(f,0,SEEK_END); fileSize=ftell(f); fseek(f,0,SEEK_SET);
    fileBuf=malloc(fileSize); fread(fileBuf,1,fileSize,f); fclose(f);

    unsigned char *ptr=fileBuf+HEADER_SIZE;
    size_t perTile = TEX_SIZE*TEX_SIZE
                   + (TEX_SIZE/2)*(TEX_SIZE/2)
                   + (TEX_SIZE/4)*(TEX_SIZE/4)
                   + (TEX_SIZE/8)*(TEX_SIZE/8)
                   + UNKNOWN_BYTES;
    for(int i=0;i<MAX_FLOORS;i++){
        memcpy(floorIdx[i],ptr,TEX_SIZE*TEX_SIZE);
        floorRGBA[i]=malloc(TEX_SIZE*TEX_SIZE*4);
        for(int p=0;p<TEX_SIZE*TEX_SIZE;p++){
            unsigned char ci=floorIdx[i][p];
            floorRGBA[i][4*p+0]=palette[ci][0];
            floorRGBA[i][4*p+1]=palette[ci][1];
            floorRGBA[i][4*p+2]=palette[ci][2];
            floorRGBA[i][4*p+3]=255;
        }
        ptr+=perTile;
    }
    for(int i=0;i<MAX_FLOORS;i++){
        int w1,h1,w2,h2;
        floorMip1Data[i]=generateMip(floorRGBA[i],TEX_SIZE,TEX_SIZE,&w1,&h1);
        floorMip2Data[i]=generateMip(floorMip1Data[i],w1,h1,&w2,&h2);
        floorMip3Data[i]=generateMip(floorMip2Data[i],w2,h2,&w2,&h2);
    }
}

void saveGridPNG(){
    char base[256]; buildBase(base);
    char out[512]; sprintf(out,"%s.png",base);
    unsigned char *buf=malloc(512*512*4);
    for(int y=0;y<8;y++)for(int x=0;x<8;x++){
        unsigned char *t=floorRGBA[y*8+x];
        for(int py=0;py<64;py++)for(int px=0;px<64;px++){
            int dy=y*64+py, dx=x*64+px;
            memcpy(buf+(dy*512+dx)*4,t+(py*64+px)*4,4);
        }
    }
    if(stbi_write_png(out,512,512,4,buf,512*4))
        printf("Saved grid as %s\n",out);
    else fprintf(stderr,"fail %s\n",out);
    free(buf);
}

void saveTilesPNG(){
    char base[256]; buildBase(base);
    MKDIR(base);
    char mf[512]; sprintf(mf,"%s/%s.txt",base,base);
    FILE *f=fopen(mf,"w");
    for(int i=0;i<MAX_FLOORS;i++){
        char fn[512]; sprintf(fn,"%s/%s_%02d.png",base,base,i);
        stbi_write_png(fn,64,64,4,floorRGBA[i],64*4);
        fprintf(f,"%s_%02d.png\n",base,i);
    }
    fclose(f);
    printf("Saved tiles in %s/, manifest %s\n",base,mf);
}

void copyTileToClipboard(){
#ifdef _WIN32
    int idx=sel.y*8+sel.x; unsigned char *t=floorRGBA[idx];
    BITMAPINFOHEADER bi={0};
    bi.biSize=sizeof(bi); bi.biWidth=TEX_SIZE; bi.biHeight=TEX_SIZE;
    bi.biPlanes=1; bi.biBitCount=24; bi.biCompression=BI_RGB;
    bi.biSizeImage=TEX_SIZE*TEX_SIZE*3;
    int total=sizeof(bi)+bi.biSizeImage;
    HGLOBAL hD=GlobalAlloc(GHND|GMEM_DDESHARE,total);
    unsigned char *p=GlobalLock(hD); memcpy(p,&bi,sizeof(bi));
    unsigned char *pix=p+sizeof(bi);
    for(int y=0;y<TEX_SIZE;y++){
        unsigned char *dst=pix+(TEX_SIZE-1-y)*TEX_SIZE*3;
        unsigned char *src=t+y*TEX_SIZE*4;
        for(int x=0;x<TEX_SIZE;x++){
            dst[x*3+0]=src[x*4+2];
            dst[x*3+1]=src[x*4+1];
            dst[x*3+2]=src[x*4+0];
        }
    }
    GlobalUnlock(hD);
    if(OpenClipboard(NULL)){ EmptyClipboard(); SetClipboardData(CF_DIB,hD); CloseClipboard(); printf("Copied tile %d\n",idx);}
    else GlobalFree(hD);
#else
    fprintf(stderr,"Clipboard not supported\n");
#endif
}

void pasteTileFromClipboard(){
#ifdef _WIN32
    if(!IsClipboardFormatAvailable(CF_DIB)) return;
    if(!OpenClipboard(NULL)) return;
    HGLOBAL h=GetClipboardData(CF_DIB); if(!h){CloseClipboard();return;}
    unsigned char *p=GlobalLock(h); BITMAPINFOHEADER *bi=(BITMAPINFOHEADER*)p;
    if(bi->biWidth!=TEX_SIZE||abs(bi->biHeight)!=TEX_SIZE){GlobalUnlock(h);CloseClipboard();return;}
    unsigned char *pix=p+bi->biSize;
    unsigned char *buf=malloc(TEX_SIZE*TEX_SIZE*4);
    for(int y=0;y<TEX_SIZE;y++){
      int row=(bi->biHeight>0?(TEX_SIZE-1-y):y)*TEX_SIZE*3;
      for(int x=0;x<TEX_SIZE;x++){
        unsigned char B=pix[row+x*3+0],G=pix[row+x*3+1],R=pix[row+x*3+2];
        int pi=y*TEX_SIZE+x;
        buf[4*pi+0]=R; buf[4*pi+1]=G; buf[4*pi+2]=B; buf[4*pi+3]=255;
      }
    }
    GlobalUnlock(h); CloseClipboard();
    applyDither(buf,TEX_SIZE,TEX_SIZE);
    unsigned char newRGBA[64*64*4], newIdx[64*64];
    for(int p=0;p<64*64;p++){
        unsigned char R=buf[4*p+0],G=buf[4*p+1],B=buf[4*p+2];
        int best=0,bd=INT_MAX;
        for(int c=0;c<256;c++){
            int dr=R-palette[c][0], dg=G-palette[c][1], db=B-palette[c][2];
            int d=dr*dr+dg*dg+db*db;
            if(d<bd){bd=d;best=c;}
        }
        newIdx[p]=best;
        newRGBA[4*p+0]=palette[best][0];
        newRGBA[4*p+1]=palette[best][1];
        newRGBA[4*p+2]=palette[best][2];
        newRGBA[4*p+3]=255;
    }
    free(buf);
    int ti=sel.y*8+sel.x;
    pushUndoState(ti);
    memcpy(floorRGBA[ti],newRGBA,64*64*4);
    memcpy(floorIdx [ti],newIdx ,64*64);
    int w1,h1,w2,h2;
    free(floorMip1Data[ti]); free(floorMip2Data[ti]); free(floorMip3Data[ti]);
    floorMip1Data[ti]=generateMip(floorRGBA[ti],64,64,&w1,&h1);
    floorMip2Data[ti]=generateMip(floorMip1Data[ti],w1,h1,&w2,&h2);
    floorMip3Data[ti]=generateMip(floorMip2Data[ti],w2,h2,&w2,&h2);
    updateTileTextures(ti);
    dirty=true; glutPostRedisplay();
#else
    fprintf(stderr,"Clipboard not supported\n");
#endif
}

void importGridPNG(){
    char base[256]; buildBase(base);
    char in[512]; sprintf(in,"%s.png",base);
    int W,H,n;
    unsigned char *img=stbi_load(in,&W,&H,&n,4);
    if(!img){fprintf(stderr,"load %s\n",in);return;}
    if(W!=512||H!=512){fprintf(stderr,"need 512×512\n");stbi_image_free(img);return;}
    applyDither(img,512,512);
    for(int ty=0;ty<8;ty++)for(int tx=0;tx<8;tx++){
      unsigned char newRGBA[64*64*4], newIdx[64*64];
      for(int py=0;py<64;py++)for(int px=0;px<64;px++){
        int sx=tx*64+px, sy=ty*64+py;
        int p=(sy*512+ sx)*4;
        unsigned char R=img[p],G=img[p+1],B=img[p+2];
        newRGBA[4*(py*64+px)+0]=R;
        newRGBA[4*(py*64+px)+1]=G;
        newRGBA[4*(py*64+px)+2]=B;
        newRGBA[4*(py*64+px)+3]=255;
      }
      for(int p=0;p<64*64;p++){
        unsigned char R=newRGBA[4*p+0],G=newRGBA[4*p+1],B=newRGBA[4*p+2];
        int best=0,bd=INT_MAX;
        for(int c=0;c<256;c++){
            int dr=R-palette[c][0], dg=G-palette[c][1], db=B-palette[c][2];
            int d=dr*dr+dg*dg+db*db;
            if(d<bd){bd=d;best=c;}
        }
        newIdx[p]=best;
        newRGBA[4*p+0]=palette[best][0];
        newRGBA[4*p+1]=palette[best][1];
        newRGBA[4*p+2]=palette[best][2];
      }
      int ti=ty*8+tx;
      pushUndoState(ti);
      memcpy(floorRGBA[ti],newRGBA,64*64*4);
      memcpy(floorIdx [ti],newIdx ,64*64);
    }
    stbi_image_free(img);
    for(int ti=0;ti<MAX_FLOORS;ti++){
      int w1,h1,w2,h2;
      free(floorMip1Data[ti]); free(floorMip2Data[ti]); free(floorMip3Data[ti]);
      floorMip1Data[ti]=generateMip(floorRGBA[ti],64,64,&w1,&h1);
      floorMip2Data[ti]=generateMip(floorMip1Data[ti],w1,h1,&w2,&h2);
      floorMip3Data[ti]=generateMip(floorMip2Data[ti],w2,h2,&w2,&h2);
      updateTileTextures(ti);
    }
    dirty=true; 
    glutPostRedisplay();
    glutSwapBuffers();
}

void importTilesFromManifest(){
    char base[256]; buildBase(base);
    char folder[512]; sprintf(folder,"%s",base);
    char mfpath[512]; sprintf(mfpath,"%s/%s.txt",folder,base);
    FILE *mf=fopen(mfpath,"r"); if(!mf) return;
    char line[512];
    
    while(fgets(line,sizeof(line),mf)){
        char *nl=strchr(line,'\n'); if(nl)*nl='\0';
        char filepath[1024]; sprintf(filepath,"%s/%s",folder,line);
        int W,H,n; unsigned char *img=stbi_load(filepath,&W,&H,&n,4);
        if(!img||W!=64||H!=64){ if(img)stbi_image_free(img); continue; }
        
        applyDither(img,64,64);
        unsigned char newRGBA[64*64*4], newIdx[64*64];
        
        for(int p=0;p<64*64;p++){
            unsigned char R=img[4*p+0],G=img[4*p+1],B=img[4*p+2];
            int best=0,bd=INT_MAX;
            for(int c=0;c<256;c++){
                int dr=R-palette[c][0], dg=G-palette[c][1], db=B-palette[c][2];
                int d=dr*dr+dg*dg+db*db;
                if(d<bd){bd=d;best=c;}
            }
            newIdx[p]=best;
            newRGBA[4*p+0]=palette[best][0];
            newRGBA[4*p+1]=palette[best][1];
            newRGBA[4*p+2]=palette[best][2];
            newRGBA[4*p+3]=255;
        }
        
        stbi_image_free(img);
        char *us=strrchr(line,'_'), *dot=strrchr(line,'.');
        if(!us||!dot) continue;
        int ti=atoi(us+1); if(ti<0||ti>=MAX_FLOORS) continue;
        
        pushUndoState(ti);
        memcpy(floorRGBA[ti],newRGBA,64*64*4);
        memcpy(floorIdx [ti],newIdx ,64*64);
        
        int w1,h1,w2,h2;
        free(floorMip1Data[ti]); free(floorMip2Data[ti]); free(floorMip3Data[ti]);
        floorMip1Data[ti]=generateMip(floorRGBA[ti],64,64,&w1,&h1);
        floorMip2Data[ti]=generateMip(floorMip1Data[ti],w1,h1,&w2,&h2);
        floorMip3Data[ti]=generateMip(floorMip2Data[ti],w2,h2,&w2,&h2);
        updateTileTextures(ti);
    }
    fclose(mf);
    
    dirty=true; 
    glutPostRedisplay();
    glutSwapBuffers();
}

void importSelectedTile(){
    char base[256]; buildBase(base);
    char folder[512]; sprintf(folder,"%s",base);
    char fn[512]; sprintf(fn,"%s/%s_%02d.png",folder,base,sel.y*8+sel.x);
    
    int W,H,n;
    unsigned char *img=stbi_load(fn,&W,&H,&n,4);
    if(!img || W!=64 || H!=64){
        if(img) stbi_image_free(img);
        fprintf(stderr,"Failed to load %s\n",fn);
        return;
    }
    
    applyDither(img,64,64);
    unsigned char newRGBA[64*64*4], newIdx[64*64];
    
    for(int p=0;p<64*64;p++){
        unsigned char R=img[4*p+0],G=img[4*p+1],B=img[4*p+2];
        int best=0,bd=INT_MAX;
        for(int c=0;c<256;c++){
            int dr=R-palette[c][0], dg=G-palette[c][1], db=B-palette[c][2];
            int d=dr*dr+dg*dg+db*db;
            if(d<bd){bd=d;best=c;}
        }
        newIdx[p]=best;
        newRGBA[4*p+0]=palette[best][0];
        newRGBA[4*p+1]=palette[best][1];
        newRGBA[4*p+2]=palette[best][2];
        newRGBA[4*p+3]=255;
    }
    
    stbi_image_free(img);
    int ti=sel.y*8+sel.x;
    pushUndoState(ti);
    memcpy(floorRGBA[ti],newRGBA,64*64*4);
    memcpy(floorIdx [ti],newIdx ,64*64);
    
    int w1,h1,w2,h2;
    free(floorMip1Data[ti]); free(floorMip2Data[ti]); free(floorMip3Data[ti]);
    floorMip1Data[ti]=generateMip(floorRGBA[ti],64,64,&w1,&h1);
    floorMip2Data[ti]=generateMip(floorMip1Data[ti],w1,h1,&w2,&h2);
    floorMip3Data[ti]=generateMip(floorMip2Data[ti],w2,h2,&w2,&h2);
    updateTileTextures(ti);
    
    dirty=true;
    glutPostRedisplay();
    glutSwapBuffers();
}

void saveFileXX(){
    size_t baseSz=TEX_SIZE*TEX_SIZE;
    size_t m1=(TEX_SIZE/2)*(TEX_SIZE/2),
           m2=(TEX_SIZE/4)*(TEX_SIZE/4),
           m3=(TEX_SIZE/8)*(TEX_SIZE/8);
    size_t perTile=baseSz+m1+m2+m3+UNKNOWN_BYTES;
    for(int i=0;i<MAX_FLOORS;i++){
        unsigned char *dst=fileBuf+HEADER_SIZE+i*perTile;
        memcpy(dst,floorIdx[i],baseSz);
        unsigned char *p1=dst+baseSz;
        for(int p=0;p<m1;p++){
            unsigned char *px=floorMip1Data[i]+4*p;
            int best=0,bd=INT_MAX;
            for(int c=0;c<256;c++){
                int dr=px[0]-palette[c][0], dg=px[1]-palette[c][1], db=px[2]-palette[c][2];
                int d=dr*dr+dg*dg+db*db; if(d<bd){bd=d;best=c;}
            }
            p1[p]=best;
        }
        unsigned char *p2=p1+m1;
        for(int p=0;p<m2;p++){
            unsigned char *px=floorMip2Data[i]+4*p;
            int best=0,bd=INT_MAX;
            for(int c=0;c<256;c++){
                int dr=px[0]-palette[c][0], dg=px[1]-palette[c][1], db=px[2]-palette[c][2];
                int d=dr*dr+dg*dg+db*db; if(d<bd){bd=d;best=c;}
            }
            p2[p]=best;
        }
        unsigned char *p3=p2+m2;
        for(int p=0;p<m3;p++){
            unsigned char *px=floorMip3Data[i]+4*p;
            int best=0,bd=INT_MAX;
            for(int c=0;c<256;c++){
                int dr=px[0]-palette[c][0], dg=px[1]-palette[c][1], db=px[2]-palette[c][2];
                int d=dr*dr+dg*dg+db*db; if(d<bd){bd=d;best=c;}
            }
            p3[p]=best;
        }
    }
    FILE *f=fopen(g_filename,"wb");
    if(!f){perror("saveFileXX");return;}
    fwrite(fileBuf,1,fileSize,f);
    fclose(f);
    dirty=false; printf("Saved %s\n",g_filename);
}

void setClearColorFromPalette(){
    unsigned char *c=palette[bgIndex];
    glClearColor(c[0]/255.0f,c[1]/255.0f,c[2]/255.0f,1.0f);
}

void initGL(){
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
    glGenTextures(MAX_FLOORS,texBase);
    glGenTextures(MAX_FLOORS,texMip1);
    glGenTextures(MAX_FLOORS,texMip2);
    glGenTextures(MAX_FLOORS,texMip3);
    for(int i=0;i<MAX_FLOORS;i++){
        glBindTexture(GL_TEXTURE_2D,texBase[i]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,floorRGBA[i]);

        glBindTexture(GL_TEXTURE_2D,texMip1[i]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,32,32,0,GL_RGBA,GL_UNSIGNED_BYTE,floorMip1Data[i]);

        glBindTexture(GL_TEXTURE_2D,texMip2[i]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,16,16,0,GL_RGBA,GL_UNSIGNED_BYTE,floorMip2Data[i]);

        glBindTexture(GL_TEXTURE_2D,texMip3[i]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,8,8,0,GL_RGBA,GL_UNSIGNED_BYTE,floorMip3Data[i]);
    }
    setClearColorFromPalette();
    computeViewport(windowW,windowH);
    glViewport(viewportX,viewportY,viewportW,viewportH);
}

void computeViewport(int w,int h){
    windowW=w; windowH=h;
    int side=w<h?w:h;
    if(side<MIN_WINDOW_W) side=MIN_WINDOW_W;
    if(side>w) side=w;
    if(side>h) side=h;
    viewportW=viewportH=side;
    viewportX=w-side;
    viewportY=(h-side)/2;
}

void drawText(const char *s,int x,int y){
    glRasterPos2i(x,y);
    while(*s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10,*s++);
}

void renderOverlay(){
    glViewport(0,0,windowW,windowH);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0,windowW,0,windowH,-1,1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glColor3f(1,1,1);

    if(showUI){
    char buf[64];
    const char *fn=strrchr(g_filename,'/')?strrchr(g_filename,'/')+1:g_filename;
    fn=strrchr(fn,'\\')?strrchr(fn,'\\')+1:fn;
    
    // File Info
    drawText("FILE INFO",10,windowH-20);
    sprintf(buf,"File name: %s",fn); drawText(buf,10,windowH-35);
    sprintf(buf,"Tiles: %d",MAX_FLOORS);       drawText(buf,10,windowH-50);
    sprintf(buf,"Selected tile: %d",sel.y*8+sel.x);drawText(buf,10,windowH-65);
    sprintf(buf,"(Not implemented) Flag: %d",tileFlags[sel.y*8+sel.x]); drawText(buf,10,windowH-80);
    
    // Controls
    drawText("CONTROLS",10,windowH-105);
    const char *ctrls[] = {
        "F1 : Toggle UI","T   : Tile Pattern view","M   :Mipmap view",
        "Arrows : Navigate","+/- : Zoom","ESC : Reset GUI",
        "PgUp/Dn: Change BG color", "U : Undo (100x)"
    };
    for(int i=0;i<8;i++) drawText(ctrls[i],10,windowH-120 - i*15);
    
    // Import/Export
    drawText("IMPORT/EXPORT",10,windowH-255);
    const char *io[] = {
        "F5  : Save 512x512 grid to PNG","F6  : Save all 64x64 tiles to folder as PNG",
        "F9  : Import 512x512 grid from PNG","F10 : Import all PNG tiles from folder",
        "F11 : Import selected PNG tile from folder","DEL : Select import dither method"
    };
    for(int i=0;i<6;i++) drawText(io[i],10,windowH-270 - i*15);
    
    // Dither Info
    sprintf(buf,"DITHER METHOD: %s",ditherNames[importDitherMode]);
    drawText(buf,10,windowH-370);
    
    // Tile Operations
    drawText("TILE OPTIONS",10,windowH-395);
    drawText("C : Copy tile",10,windowH-410);
    drawText("V : Paste tile",10,windowH-425);
    drawText("F : Flip vertical",10,windowH-440);
    drawText("H : Flip horizontal",10,windowH-455);
    drawText("R : Rotate 90°",10,windowH-470);
}

    if(dirty){
        drawText("PRESS F12 TO SAVE CHANGES",10,10);
    }

    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
}

void display(){
    glViewport(0,0,windowW,windowH);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(viewportX,viewportY,viewportW,viewportH);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    float gp=(viewportW>=MIN_WINDOW_W&&viewportH>=MIN_WINDOW_H)?MIN_WINDOW_W:(float)(viewportW<viewportH?viewportW:viewportH);
    float baseNDC=(gp/viewportW)*2.0f;
    float gridNDC=(mode==MODE_GRID? baseNDC*zoomLevel: baseNDC);
    float half=gridNDC*0.5f, step=gridNDC/8.0f;

    if(mode==MODE_GRID){
        for(int y=0;y<8;y++)for(int x=0;x<8;x++){
            int idx=y*8+x;
            float vx=-half+x*step, vy=half-(y+1)*step;
            glBindTexture(GL_TEXTURE_2D,texBase[idx]);
            glBegin(GL_QUADS);
              glTexCoord2f(0,1); glVertex2f(vx,vy);
              glTexCoord2f(1,1); glVertex2f(vx+step,vy);
              glTexCoord2f(1,0); glVertex2f(vx+step,vy+step);
              glTexCoord2f(0,0); glVertex2f(vx,vy+step);
            glEnd();
        }
        glColor3f(1,0,0); glLineWidth(1); glBegin(GL_LINE_LOOP);
          float x0=-half+sel.x*step, y0=half-sel.y*step;
          glVertex2f(x0,y0); glVertex2f(x0+step,y0);
          glVertex2f(x0+step,y0-step); glVertex2f(x0,y0-step);
        glEnd(); glColor3f(1,1,1);
    }
    else if(mode==MODE_TILE){
        int idx=sel.y*8+sel.x;
        glBindTexture(GL_TEXTURE_2D,texBase[idx]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        float rx=(viewportW/(float)TEX_SIZE)*zoomLevel,
              ry=(viewportH/(float)TEX_SIZE)*zoomLevel;
        glBegin(GL_QUADS);
          glTexCoord2f(0,ry);  glVertex2f(-1,-1);
          glTexCoord2f(rx,ry); glVertex2f( 1,-1);
          glTexCoord2f(rx,0);  glVertex2f( 1, 1);
          glTexCoord2f(0,0);   glVertex2f(-1, 1);
        glEnd();
    }
    else {
        GLuint *lvl[4]={texBase,texMip1,texMip2,texMip3};
        for(int i=0;i<4;i++){
            GLuint t=lvl[i][sel.y*8+sel.x];
            glBindTexture(GL_TEXTURE_2D,t);
            int qx=i%2, qy=i/2;
            float x0=-1+qx, y0=1-qy, x1=x0+1, y1=y0-1;
            glBegin(GL_QUADS);
              glTexCoord2f(0,1); glVertex2f(x0,y1);
              glTexCoord2f(1,1); glVertex2f(x1,y1);
              glTexCoord2f(1,0); glVertex2f(x1,y0);
              glTexCoord2f(0,0); glVertex2f(x0,y0);
            glEnd();
        }
    }

    renderOverlay();
    glutSwapBuffers();
}

void reshape(int w,int h){
    int nw=w<MIN_WINDOW_W?MIN_WINDOW_W:w;
    int nh=h<MIN_WINDOW_H?MIN_WINDOW_H:h;
    if(nw!=w||nh!=h){ glutReshapeWindow(nw,nh); return; }
    computeViewport(nw,nh);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);  glLoadIdentity();
}

void onSpecialKey(int k,int x,int y){
    switch(k){
        case GLUT_KEY_F1:        showUI=!showUI; break;
        case GLUT_KEY_F5:        saveGridPNG(); break;
        case GLUT_KEY_F6:        saveTilesPNG(); break;
        case GLUT_KEY_F9:        importGridPNG(); break;
        case GLUT_KEY_F10:       importTilesFromManifest(); break;
        case GLUT_KEY_F11:       importSelectedTile(); break;
        case GLUT_KEY_F12:       if(dirty) saveFileXX(); break;
        case GLUT_KEY_PAGE_UP:   bgIndex=(bgIndex+1)&0xFF; setClearColorFromPalette(); break;
        case GLUT_KEY_PAGE_DOWN: bgIndex=(bgIndex-1)&0xFF; setClearColorFromPalette(); break;
        case GLUT_KEY_LEFT:      sel.x=(sel.x+7)%8; break;
        case GLUT_KEY_RIGHT:     sel.x=(sel.x+1)%8; break;
        case GLUT_KEY_UP:        sel.y=(sel.y+7)%8; break;
        case GLUT_KEY_DOWN:      sel.y=(sel.y+1)%8; break;
    }
    glutPostRedisplay();
}

void onAsciiKey(unsigned char key,int x,int y){
    switch(key){
        case 27: // ESC key
            mode=MODE_GRID; zoomLevel=1.0f; sel.x=sel.y=0;
            bgIndex=defaultBgIndex; setClearColorFromPalette();
            glutReshapeWindow(DEFAULT_WINDOW_W,DEFAULT_WINDOW_H);
            dirty=false;
            break;
        case '+': zoomLevel+=zoomStep;
            { float rp=MIN_WINDOW_W*zoomLevel;
            if(rp>windowW||rp>windowH)
                glutReshapeWindow((int)rp,(int)rp);
            } break;
        case '-': zoomLevel=(zoomLevel-zoomStep>1.0f)?zoomLevel-zoomStep:1.0f; break;
        case 't': case 'T': mode=(mode==MODE_TILE)?MODE_GRID:MODE_TILE; break;
        case 'm': case 'M': mode=(mode==MODE_MIPMAP)?MODE_GRID:MODE_MIPMAP; break;
        case 'c': case 'C': copyTileToClipboard(); break;
        case 'v': case 'V': pasteTileFromClipboard(); break;
        case 'f': case 'F': flipTileVertical(); break;
        case 'h': case 'H': flipTileHorizontal(); break;
        case 'r': case 'R': rotateTile90(); break;
        case 'u': case 'U': undoLastAction(); break;
        case '\x7f': // DEL
            importDitherMode = (importDitherMode + 1) % DITHER_COUNT;
            glutPostRedisplay();
            break;
    }
    glutPostRedisplay();
}

void onMouse(int b,int s,int mx,int my){
    if(b==GLUT_LEFT_BUTTON&&s==GLUT_DOWN&&mode==MODE_GRID){
        int ry=windowH-1-my;
        float gp=(viewportW>=MIN_WINDOW_W&&viewportH>=MIN_WINDOW_H)?MIN_WINDOW_W:(float)(viewportW<viewportH?viewportW:viewportH);
        float rp=gp*zoomLevel;
        float ox=viewportX+(viewportW-rp)/2, oy=viewportY+(viewportH-rp)/2;
        if(mx<ox||mx>=ox+rp||ry<oy||ry>=oy+rp) return;
        float rx=(mx-ox)/rp, ryf=(ry-oy)/rp;
        int tx=(int)(rx*8), ty=7-(int)(ryf*8);
        if(tx>=0&&tx<8&&ty>=0&&ty<8){
            sel.x=tx; sel.y=ty; glutPostRedisplay();
        }
    }
}

int main(int argc,char **argv){
    g_filename = (argc>1)? argv[1] : "FLOORS.XX";
    srand(12345);
    loadPalette("chasmpalette.act");
    loadFloors(g_filename);

    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA);
    glutInitWindowSize(windowW,windowH);
    glutCreateWindow("Chasm The Rift Floor Toolkit v1.0.2 by SMR9000");

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutSpecialFunc(onSpecialKey);
    glutKeyboardFunc(onAsciiKey);
    glutMouseFunc(onMouse);

    initGL();
    glutMainLoop();
    return 0;
}