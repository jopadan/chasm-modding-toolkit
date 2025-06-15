// Version: 1.0.0 (105-FINAL)

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <direct.h>    // _mkdir
#else
  #include <sys/stat.h>  // mkdir
#endif

#include <GL/freeglut.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

// STB Image Write & Read
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Forward declarations
void create_textures(void);
void import_manifest(void);

// Globals
static uint8_t  palette[256][3];    // ACT palette
static uint8_t *frame_data = NULL;  // raw indices
static GLuint  *textures   = NULL;  // GL textures
static unsigned frame_count   = 0;
static unsigned width_px      = 0;
static unsigned height_px     = 0;
static unsigned current_frame = 0;
static int      fps           = 12;
static const int default_fps  = 12;
static float    zoom          = 5.0f;
static bool     mask_transparent = false;
static uint8_t  bg_index      = 2; // default background index

// Paths
static char spr_path[512]    = "";
static char export_name[256] = "";

int window_width  = 800;
int window_height = 600;
const int MIN_WIN_W = 200;
const int MIN_WIN_H = 150;

// cross-platform mkdir
static void make_dir(const char *p) {
#ifdef _WIN32
    _mkdir(p);
#else
    mkdir(p, 0755);
#endif
}

// Load ACT palette
bool load_palette(void) {
    FILE *f = fopen("chasmpalette.act","rb");
    if (!f) return false;
    if (fread(palette,3,256,f)!=256) { fclose(f); return false; }
    fclose(f);
    return true;
}

// Load SPR
bool load_spr(const char *fn) {
    strcpy(spr_path, fn);
    FILE *f = fopen(fn,"rb");
    if (!f) return false;
    uint16_t fc,w,h;
    if (fread(&fc,2,1,f)!=1 || fread(&w,2,1,f)!=1 || fread(&h,2,1,f)!=1) {
        fclose(f); return false;
    }
    frame_count = fc;
    width_px    = w;
    height_px   = h;
    size_t fs   = (size_t)w*h;
    frame_data  = malloc(frame_count*fs);
    if (!frame_data) { fclose(f); return false; }
    for (unsigned i=0; i<frame_count; ++i) {
        if (fread(frame_data + i*fs,1,fs,f)!=fs) {
            fclose(f); return false;
        }
    }
    fclose(f);
    return true;
}

// Export frames + manifest
void export_frames(void) {
    make_dir(export_name);
    char manifest[512];
    snprintf(manifest,sizeof(manifest), "%s/%s.txt", export_name, export_name);
    FILE *mf = fopen(manifest,"w");
    if (!mf) printf("Failed manifest %s\n",manifest);
    size_t fs = (size_t)width_px*height_px;
    uint8_t *rgba = malloc(fs*4);
    for (unsigned i=0; i<frame_count; ++i) {
        uint8_t *src = frame_data + i*fs;
        for (size_t p=0; p<fs; ++p) {
            uint8_t c = src[p];
            rgba[p*4+0] = palette[c][0];
            rgba[p*4+1] = palette[c][1];
            rgba[p*4+2] = palette[c][2];
            rgba[p*4+3] = (mask_transparent && c==0) ? 0 : 255;
        }
        char outfn[512];
        snprintf(outfn,sizeof(outfn), "%s/%s_%02u.png",
                 export_name, export_name, i+1);
        stbi_write_png(outfn, width_px, height_px, 4, rgba, width_px*4);
        printf("Exported %s\n", outfn);
        if (mf) fprintf(mf, "%s_%02u.png\n", export_name, i+1);
    }
    free(rgba);
    if (mf) { fclose(mf); printf("Manifest %s\n",manifest); }
}

// Nearest-palette lookup
int find_palette_index(uint8_t r,uint8_t g,uint8_t b){
    for (int p=0; p<256; ++p)
        if (palette[p][0]==r && palette[p][1]==g && palette[p][2]==b)
            return p;
    int best=0, bestD=INT_MAX;
    for (int p=0; p<256; ++p) {
        int dr=(int)palette[p][0]-r, dg=(int)palette[p][1]-g, db=(int)palette[p][2]-b;
        int d = dr*dr + dg*dg + db*db;
        if (d < bestD) { bestD = d; best = p; }
    }
    return best;
}

// Import from manifest + save SPR
void import_manifest(void){
    char manifest[512];
    snprintf(manifest,sizeof(manifest), "%s/%s.txt", export_name, export_name);
    FILE *mf = fopen(manifest,"r");
    if (!mf) { printf("Manifest not found %s\n",manifest); return; }
    size_t fs = (size_t)width_px*height_px;
    for (unsigned i=0; i<frame_count; ++i) {
        char fn[512];
        if (!fgets(fn,sizeof(fn),mf)) break;
        fn[strcspn(fn,"\r\n")] = 0;
        char imgf[512];
        snprintf(imgf,sizeof(imgf), "%s/%s", export_name, fn);
        int w,h,ch;
        uint8_t *img = stbi_load(imgf, &w, &h, &ch, 4);
        if (!img) { printf("Load fail %s\n",imgf); continue; }
        if (w!=(int)width_px || h!=(int)height_px) {
            printf("Size mismatch %s (%dx%d)\n", imgf, w, h);
            stbi_image_free(img);
            continue;
        }
        uint8_t *dst = frame_data + i*fs;
        for (int yy=0; yy<h; ++yy) {
            for (int xx=0; xx<w; ++xx) {
                int si = (yy*w + xx)*4;
                uint8_t r=img[si], g=img[si+1], b=img[si+2];
                int pi = find_palette_index(r,g,b);
                int di = yy*w + xx;  // no flip here
                dst[di] = (uint8_t)pi;
            }
        }
        stbi_image_free(img);
        printf("Reimported %u from %s\n", i+1, imgf);
    }
    fclose(mf);
    FILE *outf = fopen(spr_path,"wb");
    if (!outf) { printf("Save fail %s\n",spr_path); return; }
    fwrite(&frame_count,2,1,outf);
    fwrite(&width_px,   2,1,outf);
    fwrite(&height_px,  2,1,outf);
    fwrite(frame_data,  1,frame_count*fs,outf);
    fclose(outf);
    printf("Saved SPR %s\n",spr_path);
    create_textures();
    glutPostRedisplay();
}

// Create GL textures from frame_data (flip vertically)
void create_textures(void){
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    if (textures) { glDeleteTextures(frame_count,textures); free(textures); }
    textures = malloc(sizeof(GLuint)*frame_count);
    glGenTextures(frame_count,textures);
    size_t fs = (size_t)width_px*height_px;
    for (unsigned i=0; i<frame_count; ++i) {
        uint8_t *src = frame_data + i*fs;
        uint8_t *idx = malloc(fs);
        // vertical flip for display
        for (unsigned y=0; y<height_px; ++y)
            memcpy(idx + y*width_px, src + (height_px-1-y)*width_px, width_px);
        if (mask_transparent) {
            uint8_t *buf = malloc(fs*4);
            for (size_t p=0; p<fs; ++p) {
                uint8_t c=idx[p];
                buf[p*4+0]=palette[c][0];
                buf[p*4+1]=palette[c][1];
                buf[p*4+2]=palette[c][2];
                buf[p*4+3]=(c==0?0:255);
            }
            glBindTexture(GL_TEXTURE_2D,textures[i]);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width_px,height_px,0,
                         GL_RGBA,GL_UNSIGNED_BYTE,buf);
            free(buf);
        } else {
            uint8_t *buf = malloc(fs*3);
            for (size_t p=0; p<fs; ++p) {
                uint8_t c=idx[p];
                buf[p*3+0]=palette[c][0];
                buf[p*3+1]=palette[c][1];
                buf[p*3+2]=palette[c][2];
            }
            glBindTexture(GL_TEXTURE_2D,textures[i]);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,width_px,height_px,0,
                         GL_RGB,GL_UNSIGNED_BYTE,buf);
            free(buf);
        }
        free(idx);
    }
}

// Resize
void reshape(int w,int h){
    if(w<MIN_WIN_W) w=MIN_WIN_W;
    if(h<MIN_WIN_H) h=MIN_WIN_H;
    window_width=w; window_height=h;
    glViewport(0,0,w,h);
}

// Draw controls
void draw_controls(void){
    const char *ctrls[]={
      "SPACE=Play/Pause","DEL=ToggleMask","PgUp=BG+","PgDn=BG-",
      "Up=FPS+","Down=FPS-","Left/Right=Prev/Next","ESC=Reset",
      "F5=Export","F9=Reimport"
    };
    float br=palette[bg_index][0]/255.0f,
          bg=palette[bg_index][1]/255.0f,
          bb=palette[bg_index][2]/255.0f;
    glColor3f(1-br,1-bg,1-bb);
    int y=window_height-15;
    for(int i=0;i<10;++i){
        glRasterPos2i(10,y-15*i);
        for(const char*c=ctrls[i];*c;++c)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12,*c);
    }
}

// Draw info
void draw_info(const char *info){
    float br=palette[bg_index][0]/255.0f,
          bg=palette[bg_index][1]/255.0f,
          bb=palette[bg_index][2]/255.0f;
    glColor3f(1-br,1-bg,1-bb);
    glRasterPos2i(10,10);
    for(const char*c=info;*c;++c)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12,*c);
}

// Render
void display(void){
    float br=palette[bg_index][0]/255.0f,
          bg=palette[bg_index][1]/255.0f,
          bb=palette[bg_index][2]/255.0f;
    glClearColor(br,bg,bb,1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0,window_width,0,window_height,-1,1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float w=width_px*zoom, h=height_px*zoom;
    float x0=(window_width-w)/2, y0=(window_height-h)/2;
    glColor3f(1,1,1);
    glBindTexture(GL_TEXTURE_2D,textures[current_frame]);
    glBegin(GL_QUADS);
      glTexCoord2f(0,0); glVertex2f(x0,y0);
      glTexCoord2f(1,0); glVertex2f(x0+w,y0);
      glTexCoord2f(1,1); glVertex2f(x0+w,y0+h);
      glTexCoord2f(0,1); glVertex2f(x0,y0+h);
    glEnd();
    int tx=(window_width-frame_count*width_px)/2;
    for(unsigned i=0;i<frame_count;++i){
        glBindTexture(GL_TEXTURE_2D,textures[i]);
        glBegin(GL_QUADS);
          glTexCoord2f(0,0); glVertex2f(tx+i*width_px,0);
          glTexCoord2f(1,0); glVertex2f(tx+(i+1)*width_px,0);
          glTexCoord2f(1,1); glVertex2f(tx+(i+1)*width_px,height_px);
          glTexCoord2f(0,1); glVertex2f(tx+i*width_px,height_px);
        glEnd();
    }
    glColor3ub(palette[175][0],palette[175][1],palette[175][2]);
    float ax=tx+current_frame*width_px+width_px/2,
          ay=height_px+5;
    glBegin(GL_TRIANGLES);
      glVertex2f(ax-5,ay); glVertex2f(ax+5,ay); glVertex2f(ax,ay+5);
    glEnd();
    draw_controls();
    char info[128];
    snprintf(info,sizeof(info),"Frame %u/%u  Zoom:%.1fx  FPS:%d",
             current_frame+1,frame_count,zoom,fps);
    draw_info(info);
    glutSwapBuffers();
}

// Timer
void timer(int v){
    if(fps>0){
        current_frame=(current_frame+1)%frame_count;
        glutPostRedisplay();
        glutTimerFunc(1000/fps,timer,0);
    }
}

// Keyboard
void keyboard(unsigned char key,int x,int y){
    switch(key){
      case ' ':
        if(fps>0) fps=0;
        else{ glutTimerFunc(1000/default_fps,timer,0); fps=default_fps; }
        break;
      case 127:
        mask_transparent=!mask_transparent;
        create_textures(); glutPostRedisplay();
        break;
      case 27:
        zoom=5.0f; mask_transparent=false;
        bg_index=2; current_frame=0;
        fps=default_fps; glutTimerFunc(1000/fps,timer,0);
        break;
      case '+': zoom*=1.1f; glutPostRedisplay(); break;
      case '-': zoom/=1.1f; if(zoom<1)zoom=1; glutPostRedisplay(); break;
    }
}

// Special
void special(int key,int x,int y){
    switch(key){
      case GLUT_KEY_F5: export_frames(); break;
      case GLUT_KEY_F9: import_manifest(); break;
      case GLUT_KEY_PAGE_UP:
        bg_index=(bg_index+1)&0xFF; create_textures(); break;
      case GLUT_KEY_PAGE_DOWN:
        bg_index=(bg_index-1)&0xFF; create_textures(); break;
      case GLUT_KEY_UP:    if(fps<30) fps++; break;
      case GLUT_KEY_DOWN:  if(fps>1) fps--; break;
      case GLUT_KEY_LEFT:
        current_frame=(current_frame+frame_count-1)%frame_count; break;
      case GLUT_KEY_RIGHT:
        current_frame=(current_frame+1)%frame_count; break;
    }
    glutPostRedisplay();
}

int main(int argc,char **argv){
    if(argc!=2){
        fprintf(stderr,"Usage: %s <sprite.spr>\n",argv[0]);
        return 1;
    }
    // derive export_name
    const char *fn=argv[1];
    const char *b=strrchr(fn,'/')?strrchr(fn,'/')+1:
                  (strrchr(fn,'\\')?strrchr(fn,'\\')+1:fn);
    char *d=strrchr(b,'.');
    size_t ln=d?(size_t)(d-b):strlen(b);
    memcpy(export_name,b,ln); export_name[ln]='\0';

    if(!load_spr(argv[1])||!load_palette()){
        fprintf(stderr,"Load failed.\n"); return 2;
    }
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB);
    glutInitWindowSize(window_width,window_height);
    glutCreateWindow("Chasm The Rift SPR Viewer v1.0.0 by SMR9000");
    reshape(window_width,window_height);
    create_textures();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutTimerFunc(1000/fps,timer,0);
    glutMainLoop();
    return 0;
}
