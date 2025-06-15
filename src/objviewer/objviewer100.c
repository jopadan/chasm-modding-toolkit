// x86_64-w64-mingw32-gcc objviewer100.c   -o objviewer.exe   -Iinclude   -Llib   -lfreeglut   -lopengl32   -lglu32   -lwinmm objviewer.res

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#endif

#include <GL/freeglut.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#pragma pack(push,1)
// Frame header in Chasm OBJ sprite
typedef struct {
    uint16_t size_x;
    uint16_t size_y;
    uint16_t x_center;
} FrameHeader;
#pragma pack(pop)

// Globals
static uint8_t      palette[256][3];
static uint8_t     *frame_data    = NULL;
static FrameHeader *headers       = NULL;
static GLuint      *textures      = NULL;
static unsigned     frame_count   = 0;
static unsigned     max_h         = 0;
static unsigned     current_frame = 0;
static int          fps           = 12;
static int          default_fps   = 12;
static float        zoom          = 1.0f;
static bool         mask_transparent = false;
static bool         playing       = true;
static uint8_t      bg_index      = 0;

int window_width  = 800;
int window_height = 600;
const int MIN_WIN_W = 200;
const int MIN_WIN_H = 150;

int load_palette(void) {
    FILE *f = fopen("chasmpalette.act", "rb");
    if (!f) return 0;
    if (fread(palette,3,256,f) != 256) { fclose(f); return 0; }
    fclose(f);
    return 1;
}

int load_objsprite(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    uint16_t cnt;
    if (fread(&cnt,sizeof(cnt),1,f)!=1) { fclose(f); return 0; }
    frame_count = cnt;
    headers = malloc(sizeof(FrameHeader)*frame_count);
    unsigned total=0;
    for(unsigned i=0;i<frame_count;i++){
        fread(&headers[i],sizeof(FrameHeader),1,f);
        if(headers[i].size_y>max_h) max_h=headers[i].size_y;
        total += (unsigned)headers[i].size_x*headers[i].size_y;
        fseek(f, headers[i].size_x*headers[i].size_y, SEEK_CUR);
    }
    frame_data=malloc(total);
    fseek(f,sizeof(cnt),SEEK_SET);
    uint8_t *p=frame_data;
    for(unsigned i=0;i<frame_count;i++){
        FrameHeader h; fread(&h,sizeof(h),1,f);
        size_t sz=(size_t)h.size_x*h.size_y;
        fread(p,1,sz,f);
        p+=sz;
    }
    fclose(f);
    return 1;
}

void create_textures(void) {
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    if(textures){ glDeleteTextures(frame_count,textures); free(textures); }
    textures = malloc(sizeof(GLuint)*frame_count);
    glGenTextures(frame_count, textures);
    size_t off=0;
    for(unsigned i=0;i<frame_count;i++){
        unsigned w=headers[i].size_x, h=headers[i].size_y;
        size_t sz=(size_t)w*h;
        uint8_t *src = frame_data + off;
        uint8_t *idx = malloc(sz);
        for(unsigned y=0;y<h;y++) for(unsigned x=0;x<w;x++)
            idx[y*w+x] = src[(h-1-y) + x*h];
        if(mask_transparent){
            uint8_t *rgba=malloc(sz*4);
            for(size_t p=0;p<sz;p++){
                uint8_t c=idx[p];
                rgba[p*4+0]=palette[c][0];
                rgba[p*4+1]=palette[c][1];
                rgba[p*4+2]=palette[c][2];
                rgba[p*4+3]=(c==255?0:255);
            }
            glBindTexture(GL_TEXTURE_2D,textures[i]);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
            free(rgba);
        } else {
            uint8_t *rgb=malloc(sz*3);
            for(size_t p=0;p<sz;p++){
                uint8_t c=idx[p];
                rgb[p*3+0]=palette[c][0];
                rgb[p*3+1]=palette[c][1];
                rgb[p*3+2]=palette[c][2];
            }
            glBindTexture(GL_TEXTURE_2D,textures[i]);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,rgb);
            free(rgb);
        }
        free(idx);
        off+=sz;
    }
}

void draw_text(int x,int y,const char *s){
    glColor3f(1,0,1);
    glRasterPos2i(x,y);
    for(;*s;++s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10,*s);
}

void reshape(int w,int h){
    if(w<MIN_WIN_W) w=MIN_WIN_W;
    if(h<MIN_WIN_H) h=MIN_WIN_H;
    window_width=w; window_height=h;
    glViewport(0,0,w,h);
}

void display(void){
    float br=palette[bg_index][0]/255.0f;
    float bg=palette[bg_index][1]/255.0f;
    float bb=palette[bg_index][2]/255.0f;
    glClearColor(br,bg,bb,1); glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(0,window_width,0,window_height,-1,1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    // Controls (one per line)
    draw_text(10, window_height-15, "SPACE = Play/Pause");
    draw_text(10, window_height-30, "DEL   = Toggle Mask");
    draw_text(10, window_height-45, "PgUp  = BG Color +");
    draw_text(10, window_height-60, "PgDn  = BG Color -");
    draw_text(10, window_height-75,"Up    = FPS +");
    draw_text(10, window_height-90,"Down  = FPS -");
    draw_text(10, window_height-105,"Left/Right = Prev/Next Frame");
    draw_text(10, window_height-120,"ESC   = Reset All");

    // Main frame
    unsigned i=current_frame;
    unsigned fw=headers[i].size_x, fh=headers[i].size_y;
    uint16_t pivot=headers[i].x_center;
    float w=fw*zoom, h=fh*zoom;
    float baseline=(window_height-max_h*zoom)*0.5f;
    float px=window_width*0.5f - pivot*zoom;
    float py=baseline;
    glColor3f(1,1,1);
    glBindTexture(GL_TEXTURE_2D,textures[i]);
    glBegin(GL_QUADS);
      glTexCoord2f(0,0); glVertex2f(px,   py);
      glTexCoord2f(1,0); glVertex2f(px+w, py);
      glTexCoord2f(1,1); glVertex2f(px+w, py+h);
      glTexCoord2f(0,1); glVertex2f(px,   py+h);
    glEnd();

    // Thumbnails
    int total_w=0;
    for(unsigned j=0;j<frame_count;j++) total_w+=headers[j].size_x;
    int start_x=(window_width-total_w)/2;
    int x=start_x, y=0;
    for(unsigned j=0;j<frame_count;j++){
        unsigned tw=headers[j].size_x, th=headers[j].size_y;
        if(!playing && j==current_frame){
            glColor3f(1,0,0);
            glBegin(GL_TRIANGLES);
              glVertex2f(x+tw*0.5f-5, th+5);
              glVertex2f(x+tw*0.5f+5, th+5);
              glVertex2f(x+tw*0.5f,   th   );
            glEnd();
            glColor3f(1,1,1);
        }
        glBindTexture(GL_TEXTURE_2D,textures[j]);
        glBegin(GL_QUADS);
          glTexCoord2f(0,0); glVertex2f(x,   y);
          glTexCoord2f(1,0); glVertex2f(x+tw,y);
          glTexCoord2f(1,1); glVertex2f(x+tw,y+th);
          glTexCoord2f(0,1); glVertex2f(x,   y+th);
        glEnd();
        x+=tw;
    }

    // Header data (top-right)
    char hd[64];
    snprintf(hd,sizeof(hd),"size:%ux%u  center:%u", fw, fh, pivot);
    draw_text(window_width-200, window_height-20, hd);

    // Frame info
    char info[128];
    snprintf(info,sizeof(info),
      "Frame %u/%u: %ux%u (zoom %.1fx) %s  FPS:%d",
      i+1, frame_count, fw, fh, zoom,
      playing?"[PLAY]":"[PAUSE]", fps);
    draw_text(10, window_height-180, info);

    glutSwapBuffers();
}

void timer(int v){
    if(playing) current_frame=(current_frame+1)%frame_count;
    glutPostRedisplay(); glutTimerFunc(1000/fps,timer,0);
}

void keyboard(unsigned char key,int x,int y){
    switch(key){
      case '+': zoom*=1.1f; break;
      case '-': zoom=(zoom>1?zoom/1.1f:1.0f); break;
      case ' ': playing=!playing; break;
      case 127: mask_transparent=!mask_transparent; create_textures(); break;
      case 27:  // ESC
        zoom = 1.0f;
        playing = true;
        bg_index = 0;
        fps = default_fps;
        current_frame = 0;
        mask_transparent = false;
        create_textures();
        break;
    }
}

void special(int key,int x,int y){
    switch(key){
      case GLUT_KEY_PAGE_UP:   bg_index=(bg_index+1)&0xFF; break;
      case GLUT_KEY_PAGE_DOWN: bg_index=(bg_index-1)&0xFF; break;
      case GLUT_KEY_UP:        fps++; break;
      case GLUT_KEY_DOWN:      if(fps>1) fps--; break;
      case GLUT_KEY_LEFT:      playing=false; current_frame=(current_frame+frame_count-1)%frame_count; break;
      case GLUT_KEY_RIGHT:     playing=false; current_frame=(current_frame+1)%frame_count; break;
    }
}

int main(int argc,char**argv){
    if(argc<2||argc>3){ fprintf(stderr,"Usage: %s <sprite.obj> [fps]\n",argv[0]); return 1; }
    if(argc==3) fps=default_fps=atoi(argv[2]);
    if(!load_palette()||!load_objsprite(argv[1])) return 2;
    glutInit(&argc,argv); glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB);
    glutInitWindowSize(window_width,window_height);
    glutCreateWindow("Chasm The Rift OBJ Viewer V1.0 by SMR9000");
    glClearColor(0,0,0,1);
    create_textures();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutTimerFunc(1000/fps,timer,0);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMainLoop();
    return 0;
}
