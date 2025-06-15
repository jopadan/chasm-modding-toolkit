/* 
 celviewer.c - simple Autodesk Animator 1 .CEL viewer using OpenGL/GLUT

 x86_64-w64-mingw32-gcc -O2 -std=c11   -I. -L.   -o celviewer.exe celviewer2.c   -lmingw32 -lfreeglut   -lopengl32 -lglu32   -lgdi32 -luser32 -lkernel32

 Usage:
   celviewer.exe <file.cel> [initial_zoom]

 Features:
   • Load 8-bit .CEL (header + palette + pixels)
   • Autoload chasmpalette.act or embedded palette
   • Toggle transparency mask (index 255) with SPACE
   • Zoom in/out (+ / -), Pan (arrow keys)
   • Change background color index (PgUp / PgDn)
   • Toggle pattern/tile view with P
   • Reset view (Esc)
   • On-screen filename, size, zoom, pattern status, and controls overlay
*/

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
#include <stdarg.h>
#include <math.h>

#pragma pack(push,1)
typedef struct {
    uint16_t type;
    uint16_t width, height;
    uint16_t x, y;
    uint8_t  depth, compress;
    uint32_t datasize;
    uint8_t  reserved[16];
} CelHeader;
#pragma pack(pop)

// Globals
static char     g_filename[256] = "";
static uint8_t  g_palette[256][3];
static uint8_t *g_cel_data      = NULL;
static CelHeader g_hdr;
static GLuint    g_tex          = 0;
static bool      g_mask         = false;
static bool      g_pattern      = false;
static float     g_zoom         = 1.0f;
static float     g_pan_x = 0, g_pan_y = 0;
static uint8_t   g_bg_index     = 0;
static int       g_win_w = 800, g_win_h = 600;

// Print usage and exit
static void print_usage(const char *p) {
    fprintf(stderr,
      "Usage:\n"
      "  %s <file.cel> [initial_zoom]\n", p);
}

// Try loading a global ACT palette
static bool load_palette_act(void) {
    FILE *f = fopen("chasmpalette.act","rb");
    if (!f) return false;
    if (fread(g_palette,3,256,f)!=256) { fclose(f); return false; }
    fclose(f);
    return true;
}

// Load a CEL file into memory, extract palette & pixels
static bool load_cel(const char *path) {
    // strip directories
    const char *b = strrchr(path,'/');
    if (!b) b = strrchr(path,'\\');
    if (b) strncpy(g_filename, b+1, sizeof(g_filename)-1);
    else strncpy(g_filename, path, sizeof(g_filename)-1);
    g_filename[sizeof(g_filename)-1] = '\0';

    FILE *f = fopen(path,"rb");
    if (!f) { perror("Error opening CEL"); return false; }
    if (fread(&g_hdr,sizeof(g_hdr),1,f)!=1) {
        fprintf(stderr,"Error reading CEL header\n");
        fclose(f); return false;
    }
    if (g_hdr.type!=0x9119) {
        fprintf(stderr,"Not a valid Autodesk Animator CEL\n");
        fclose(f); return false;
    }

    int w = g_hdr.width, h = g_hdr.height;
    size_t npix = (size_t)w * h;

    // Try embedded 6-bit palette
    uint8_t pal6[256][3];
    if (fread(pal6,3,256,f)==256) {
        for(int i=0;i<256;i++)
            for(int c=0;c<3;c++)
                g_palette[i][c] = (pal6[i][c]*255 + 31)/63;
    } else {
        // fallback to ACT or grayscale
        if (!load_palette_act()) {
            fprintf(stderr,"Note: no ACT or embedded palette, using grayscale\n");
            for(int i=0;i<256;i++)
                g_palette[i][0]=g_palette[i][1]=g_palette[i][2]=(uint8_t)i;
        }
        // seek past header to pixel data
        fseek(f, sizeof(g_hdr), SEEK_SET);
    }

    g_cel_data = malloc(npix);
    if (!g_cel_data || fread(g_cel_data,1,npix,f)!=npix) {
        fprintf(stderr,"Error reading CEL pixels\n");
        free(g_cel_data);
        fclose(f);
        return false;
    }
    fclose(f);
    printf("Loaded %s (%dx%d)\n", g_filename, w, h);
    return true;
}

// Build or rebuild the GL texture from cel_data + palette + mask
static void build_texture(void) {
    if (g_tex) glDeleteTextures(1,&g_tex);
    glGenTextures(1,&g_tex);
    glBindTexture(GL_TEXTURE_2D,g_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);

    int w = g_hdr.width, h = g_hdr.height;
    size_t npix = (size_t)w * h;
    bool use_alpha = g_mask;
    int fmt = use_alpha ? GL_RGBA : GL_RGB;
    int stride = use_alpha ? 4 : 3;
    uint8_t *buf = malloc(npix * stride);

    for(int y=0;y<h;y++){
      for(int x=0;x<w;x++){
        size_t idx = y*w + x;
        uint8_t c = g_cel_data[idx];
        buf[idx*stride+0] = g_palette[c][0];
        buf[idx*stride+1] = g_palette[c][1];
        buf[idx*stride+2] = g_palette[c][2];
        if(use_alpha) buf[idx*4+3] = (c==255 ? 0 : 255);
      }
    }

    glTexImage2D(GL_TEXTURE_2D,0,fmt,w,h,0,fmt,GL_UNSIGNED_BYTE,buf);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    free(buf);
}

// Render bitmap text with HELVETICA_10
static void draw_textf(float x, float y, const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap,fmt);
    vsnprintf(buf,sizeof(buf),fmt,ap);
    va_end(ap);
    glRasterPos2f(x,y);
    for(char *s=buf; *s; ++s)
      glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *s);
}

// Display callback
static void display(void) {
    // clear to bg color index
    float br = g_palette[g_bg_index][0]/255.f;
    float bg = g_palette[g_bg_index][1]/255.f;
    float bb = g_palette[g_bg_index][2]/255.f;
    glClearColor(br,bg,bb,1);
    glClear(GL_COLOR_BUFFER_BIT);

    // setup ortho
    glViewport(0,0,g_win_w,g_win_h);
    glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      glOrtho(0,g_win_w,0,g_win_h,-1,1);
    glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();

    // enable blending if mask on
    if (g_mask) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    } else {
      glDisable(GL_BLEND);
    }

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D,g_tex);
    glColor3f(1,1,1);

    float iw = g_hdr.width  * g_zoom;
    float ih = g_hdr.height * g_zoom;

    if (g_pattern) {
      // tile texture to fill window
      int cols = (int)ceil(g_win_w / iw);
      int rows = (int)ceil(g_win_h / ih);
      for(int ry = 0; ry <= rows; ry++){
        for(int cx = 0; cx <= cols; cx++){
          float x0 = cx * iw + g_pan_x;
          float y0 = ry * ih + g_pan_y;
          glBegin(GL_QUADS);
            glTexCoord2f(0,1); glVertex2f(x0,   y0);
            glTexCoord2f(1,1); glVertex2f(x0+iw,y0);
            glTexCoord2f(1,0); glVertex2f(x0+iw,y0+ih);
            glTexCoord2f(0,0); glVertex2f(x0,   y0+ih);
          glEnd();
        }
      }
    } else {
      // single centered quad
      float x = (g_win_w - iw)*0.5f + g_pan_x;
      float y = (g_win_h - ih)*0.5f + g_pan_y;
      glBegin(GL_QUADS);
        glTexCoord2f(0,1); glVertex2f(x,   y);
        glTexCoord2f(1,1); glVertex2f(x+iw,y);
        glTexCoord2f(1,0); glVertex2f(x+iw,y+ih);
        glTexCoord2f(0,0); glVertex2f(x,   y+ih);
      glEnd();
    }

    glDisable(GL_TEXTURE_2D);

    // overlay: filename, size, zoom, pattern status at top-left
    glColor3f(1,1,1);
    draw_textf(10, g_win_h - 20, "%s",       g_filename);
    draw_textf(10, g_win_h - 35, "Size: %dx%d  Zoom: %.2fx  Pattern: %s",
               g_hdr.width, g_hdr.height, g_zoom,
               g_pattern ? "On" : "Off");
    // controls hint at bottom
    draw_textf(10, 10,
      "+/- Zoom  |  Arrows Pan  |  SPACE Mask  |  PgUp/PgDn BG  |  P Pattern  |  Esc Reset");

    glutSwapBuffers();
}

// window reshape
static void reshape(int w, int h) {
    g_win_w = w; g_win_h = h;
    glutPostRedisplay();
}

// normal keys
static void keyboard(unsigned char key, int x, int y) {
    switch(key) {
      case '+': g_zoom *= 1.1f; break;
      case '-': if(g_zoom>0.1f) g_zoom /= 1.1f; break;
      case ' ':
        g_mask = !g_mask;
        build_texture();
        break;
      case 'p': case 'P':
        g_pattern = !g_pattern;
        break;
      case 27: // ESC
        g_zoom=1; g_pan_x=g_pan_y=0;
        g_mask=false; g_pattern=false; g_bg_index=0;
        build_texture();
        break;
    }
    glutPostRedisplay();
}

// arrow keys & PgUp/PgDn
static void special(int key,int x,int y) {
    switch(key) {
      case GLUT_KEY_LEFT:      g_pan_x -= 10; break;
      case GLUT_KEY_RIGHT:     g_pan_x += 10; break;
      case GLUT_KEY_UP:        g_pan_y += 10; break;
      case GLUT_KEY_DOWN:      g_pan_y -= 10; break;
      case GLUT_KEY_PAGE_UP:   g_bg_index = (g_bg_index +1)&0xFF; break;
      case GLUT_KEY_PAGE_DOWN: g_bg_index = (g_bg_index -1)&0xFF; break;
    }
    glutPostRedisplay();
}

int main(int argc,char **argv) {
    if(argc<2||argc>3){ print_usage(argv[0]); return 1; }
    if(argc==3) g_zoom = atof(argv[2]);
    if(!load_cel(argv[1])) return 1;
    // ensure palette loaded (ACT or embedded)
    load_palette_act();

    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA);
    glutInitWindowSize(g_win_w,g_win_h);
    glutCreateWindow("Chasm The Rift / Autodesk Animator CEL Viewer v1.0 by SMR9000");
    build_texture();

    glutDisplayFunc(display);
    glutReshapeFunc (reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc (special);
    glutMainLoop();
    return 0;
}
