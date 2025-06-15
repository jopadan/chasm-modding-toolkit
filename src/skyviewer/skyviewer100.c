// Version 1.0.0. (v111)
// x86_64-w64-mingw32-gcc -std=c11   -DGL_CLAMP_TO_EDGE=0x812F   -I. -I./stb   skyviewer111.c   -L./lib -lfreeglut -lglu32 -lopengl32 -lm   -o skyviewer.exe skyviewer.res
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/freeglut.h>
#include <time.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef GL_TEXTURE_CUBE_MAP
#define GL_TEXTURE_CUBE_MAP             0x8513
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X  0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X  0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y  0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y  0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z  0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z  0x851A
#define GL_TEXTURE_WRAP_R               0x8072
#endif

// viewer modes
enum { MODE_PANORAMA, MODE_CUBEMAP };
static int viewMode = MODE_PANORAMA;

// textures
static GLuint texPan = 0, texCube = 0;

// window state
static int winW = 960, winH = 480;
static int isFullscreen = 0, winPosX = 100, winPosY = 100;
static int windowedW = 960, windowedH = 480;

// mouse look
static float yaw = 0, pitch = 0;
static int lastX, lastY, dragging = 0;

// field of view
static float fov = 75;
static const float FOV_MIN = 50, FOV_MAX = 100, FOV_DEFAULT = 75;

// 2D toggles
static int mode2D = 0, cubemap2D = 0;

// auto-rotate
static int autorotating = 0;
static double lastInteraction = 0;
static const double AUTOROTATE_DELAY = 10.0, AUTOROTATE_SPEED = 3.0;

// forward-declare idle (to fix undeclared errors):
static void idle(void);

// -----------------------------------------------------
// 4×3 grid with exactly 6 entries for your six faces:
#define GRID_W 4
#define GRID_H 3
typedef struct {
    GLenum face;
    int   gx, gy;   // grid coords [0..GRID_W-1] × [0..GRID_H-1]
    int   rot;      // rotation CCW in degrees: 0,90,180,270
    int   flipH;    // 1 = flip horizontally
    int   flipV;    // 1 = flip vertically
} Tile;

static Tile gridTiles[] = {
    // face                         gx,gy, rot,flipH,flipV
    {GL_TEXTURE_CUBE_MAP_POSITIVE_X, 1,1,    0,    0,    0},  // right
    {GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 3,1,    0,    0,    0},  // left
    {GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 3,2,   90,    0,    0},  // top
    {GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 1,0,   90,    0,    0},  // bottom
    {GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 2,1,    0,    1,    0},  // front
    {GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0,1,    0,    1,    0},  // back
};
static const int tileCount = sizeof gridTiles / sizeof *gridTiles;

// ----------------------------------------------------------------
// Helpers

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

double nowSeconds() {
#ifdef _WIN32
    return (double)clock() / CLOCKS_PER_SEC;
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec*1e-9;
#endif
}

// load a 2D RGB texture from disk
static GLuint loadTexture(const char *path) {
    int w,h,n;
    unsigned char *img = stbi_load(path, &w, &h, &n, 3);
    if (!img) {
        fprintf(stderr, "Failed to load \"%s\"\n", path);
        exit(1);
    }
    GLuint T;
    glGenTextures(1, &T);
    glBindTexture(GL_TEXTURE_2D, T);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, img);
    stbi_image_free(img);
    return T;
}

// load a cubemap (6 faces) from folder
static GLuint loadCubemap(const char *folder, const char *base,
                          const char *suffix) {
    GLenum faces[6] = {
        GL_TEXTURE_CUBE_MAP_POSITIVE_X,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
    };
    GLuint T;
    glGenTextures(1, &T);
    glBindTexture(GL_TEXTURE_CUBE_MAP, T);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    char path[512];
    int w,h,n;
    for(int i=0;i<6;i++){
        snprintf(path, sizeof(path), "%s/%s%d%s",
                 folder, base, i, suffix);
        unsigned char*img = stbi_load(path,&w,&h,&n,3);
        if(!img){
            fprintf(stderr,"Failed to load \"%s\"\n", path);
            exit(1);
        }
        glTexImage2D(faces[i],0,GL_RGB,w,h,0,GL_RGB,
                     GL_UNSIGNED_BYTE,img);
        stbi_image_free(img);
    }
    return T;
}

// smoothly ease pitch back to zero
static void easePitch(){
    if(fabs(pitch) < 0.5f){
        pitch = 0;
        glutIdleFunc(NULL);
    } else {
        pitch *= 0.85f;
    }
    glutPostRedisplay();
}

// draw an inside‐out cube for cubemap
static void drawCube(){
    float s=1;
    glBegin(GL_QUADS);
    // -X
    glTexCoord3f(-s,-s,-s); glVertex3f(-s,-s,-s);
    glTexCoord3f(-s,-s, s); glVertex3f(-s,-s, s);
    glTexCoord3f(-s, s, s); glVertex3f(-s, s, s);
    glTexCoord3f(-s, s,-s); glVertex3f(-s, s,-s);
    // +X
    glTexCoord3f( s,-s, s); glVertex3f( s,-s, s);
    glTexCoord3f( s,-s,-s); glVertex3f( s,-s,-s);
    glTexCoord3f( s, s,-s); glVertex3f( s, s,-s);
    glTexCoord3f( s, s, s); glVertex3f( s, s, s);
    // +Y
    glTexCoord3f(-s, s, s); glVertex3f(-s, s, s);
    glTexCoord3f( s, s, s); glVertex3f( s, s, s);
    glTexCoord3f( s, s,-s); glVertex3f( s, s,-s);
    glTexCoord3f(-s, s,-s); glVertex3f(-s, s,-s);
    // -Y
    glTexCoord3f(-s,-s,-s); glVertex3f(-s,-s,-s);
    glTexCoord3f( s,-s,-s); glVertex3f( s,-s,-s);
    glTexCoord3f( s,-s, s); glVertex3f( s,-s, s);
    glTexCoord3f(-s,-s, s); glVertex3f(-s,-s, s);
    // +Z
    glTexCoord3f(-s,-s, s); glVertex3f(-s,-s, s);
    glTexCoord3f( s,-s, s); glVertex3f( s,-s, s);
    glTexCoord3f( s, s, s); glVertex3f( s, s, s);
    glTexCoord3f(-s, s, s); glVertex3f(-s, s, s);
    // -Z
    glTexCoord3f( s,-s,-s); glVertex3f( s,-s,-s);
    glTexCoord3f(-s,-s,-s); glVertex3f(-s,-s,-s);
    glTexCoord3f(-s, s,-s); glVertex3f(-s, s,-s);
    glTexCoord3f( s, s,-s); glVertex3f( s, s,-s);
    glEnd();
}

// draw one cubemap face in 2D with rot/flip
void drawCubemapFace2D(GLenum face,
    int x,int y,int sz,int rotDeg,int flipH,int flipV)
{
    glEnable(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texCube);

    glPushMatrix();
      // move to tile center
      glTranslatef(x + sz/2.0f, y + sz/2.0f, 0);
      if(rotDeg) glRotatef((float)rotDeg, 0,0,1);
      glScalef(flipH?-1:1, flipV?-1:1, 1);
      glTranslatef(-sz/2.0f,-sz/2.0f,0);

      glBegin(GL_QUADS);
        switch(face){
          case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
            glTexCoord3f(1,-1,-1); glVertex2i(0,0);
            glTexCoord3f(1,-1, 1); glVertex2i(sz,0);
            glTexCoord3f(1, 1, 1); glVertex2i(sz,sz);
            glTexCoord3f(1, 1,-1); glVertex2i(0,sz);
            break;
          case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
            glTexCoord3f(-1,-1, 1); glVertex2i(0,0);
            glTexCoord3f(-1,-1,-1); glVertex2i(sz,0);
            glTexCoord3f(-1, 1,-1); glVertex2i(sz,sz);
            glTexCoord3f(-1, 1, 1); glVertex2i(0,sz);
            break;
          case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
            glTexCoord3f(-1,1,-1); glVertex2i(0,0);
            glTexCoord3f( 1,1,-1); glVertex2i(sz,0);
            glTexCoord3f( 1,1, 1); glVertex2i(sz,sz);
            glTexCoord3f(-1,1, 1); glVertex2i(0,sz);
            break;
          case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
            glTexCoord3f(-1,-1, 1); glVertex2i(0,0);
            glTexCoord3f( 1,-1, 1); glVertex2i(sz,0);
            glTexCoord3f( 1,-1,-1); glVertex2i(sz,sz);
            glTexCoord3f(-1,-1,-1); glVertex2i(0,sz);
            break;
          case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
            glTexCoord3f(-1,-1,1); glVertex2i(0,0);
            glTexCoord3f( 1,-1,1); glVertex2i(sz,0);
            glTexCoord3f( 1, 1,1); glVertex2i(sz,sz);
            glTexCoord3f(-1, 1,1); glVertex2i(0,sz);
            break;
          case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            glTexCoord3f(1,-1,-1); glVertex2i(0,0);
            glTexCoord3f(-1,-1,-1); glVertex2i(sz,0);
            glTexCoord3f(-1, 1,-1); glVertex2i(sz,sz);
            glTexCoord3f(1, 1,-1); glVertex2i(0,sz);
            break;
        }
      glEnd();
    glPopMatrix();

    glDisable(GL_TEXTURE_CUBE_MAP);
}

// ----------------------------------------------------------------

static void display(){
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    // 2D cubemap grid
    if(viewMode==MODE_CUBEMAP && cubemap2D){
        int szX=winW/GRID_W, szY=winH/GRID_H;
        int sz=szX<szY?szX:szY;
        int totalW=sz*GRID_W, totalH=sz*GRID_H;
        int offX=(winW-totalW)/2, offY=(winH-totalH)/2;

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluOrtho2D(0,winW,0,winH);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        for(int i=0;i<tileCount;i++){
            Tile*t=&gridTiles[i];
            int x=offX + t->gx*sz;
            int y=offY + t->gy*sz;
            drawCubemapFace2D(t->face,x,y,sz,
                              t->rot,t->flipH,t->flipV);
        }
        glutSwapBuffers();
        return;
    }

    // panorama 2D
    if(viewMode==MODE_PANORAMA && mode2D){
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluOrtho2D(0,winW,0,winH);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D,texPan);
        glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
        glBegin(GL_QUADS);
          glTexCoord2f(0,1); glVertex2i(0,0);
          glTexCoord2f(1,1); glVertex2i(winW,0);
          glTexCoord2f(1,0); glVertex2i(winW,winH);
          glTexCoord2f(0,0); glVertex2i(0,winH);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    }
    else {
        // 3D view
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(fov,(double)winW/winH,0.1,100);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        float yR=yaw*(float)M_PI/180,
              pR=pitch*(float)M_PI/180;
        float dx=cosf(pR)*sinf(yR),
              dy=sinf(pR),
              dz=-cosf(pR)*cosf(yR);
        gluLookAt(0,0,0,dx,dy,dz,0,1,0);

        if(viewMode==MODE_PANORAMA){
            glRotatef(90,1,0,0);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D,texPan);
            glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
            GLUquadric*q=gluNewQuadric();
            gluQuadricTexture(q,1);
            gluQuadricOrientation(q,GLU_INSIDE);
            gluSphere(q,50,64,64);
            gluDeleteQuadric(q);
        } else {
            glEnable(GL_TEXTURE_CUBE_MAP);
            glBindTexture(GL_TEXTURE_CUBE_MAP,texCube);
            glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
            glPushMatrix();
            glScalef(30,30,30);
            drawCube();
            glPopMatrix();
            glDisable(GL_TEXTURE_CUBE_MAP);
        }
    }
    glutSwapBuffers();
}

static void reshape(int w,int h){
    winW=w; winH=h;
    if(!isFullscreen){windowedW=w;windowedH=h;}
    glViewport(0,0,w,h);
}

static void mouse(int b,int s,int x,int y){
    if(b==GLUT_LEFT_BUTTON){
        dragging=(s==GLUT_DOWN);
        lastX=x; lastY=y;
        if(s==GLUT_UP) glutIdleFunc(easePitch);
        autorotating=0; lastInteraction=nowSeconds();
        glutIdleFunc(idle);
    }
}
static void motion(int x,int y){
    if(!dragging) return;
    int dx=x-lastX, dy=y-lastY;
    yaw+=dx*.3f; pitch+=dy*.3f;
    if(yaw<0) yaw+=360; else if(yaw>=360) yaw-=360;
    pitch=clampf(pitch,-50,50);
    lastX=x; lastY=y;
    autorotating=0; lastInteraction=nowSeconds();
    glutPostRedisplay();
    glutIdleFunc(idle);
}

static void keyboard(unsigned char k,int x,int y){
    switch(k){
      case 'w':case'W': fov=clampf(fov-2,FOV_MIN,FOV_MAX); glutPostRedisplay();break;
      case 's':case'S': fov=clampf(fov+2,FOV_MIN,FOV_MAX); glutPostRedisplay();break;
      case 'r':case'R': fov=FOV_DEFAULT; glutPostRedisplay(); break;
      case 9: // TAB
        if(viewMode==MODE_PANORAMA) mode2D=!mode2D;
        else if(viewMode==MODE_CUBEMAP) cubemap2D=!cubemap2D;
        glutPostRedisplay();
        break;
    }
    autorotating=0; lastInteraction=nowSeconds(); glutIdleFunc(idle);
}

static void special(int k,int x,int y){
    if(k==GLUT_KEY_F12){
        if(!isFullscreen){
            winPosX=glutGet(GLUT_WINDOW_X);
            winPosY=glutGet(GLUT_WINDOW_Y);
            glutFullScreen(); isFullscreen=1;
        } else {
            glutReshapeWindow(windowedW,windowedH);
            glutPositionWindow(winPosX,winPosY);
            isFullscreen=0;
        }
    }
    autorotating=0; lastInteraction=nowSeconds(); glutIdleFunc(idle);
}

// idle: handle auto-rotate
static void idle(void){
    double now=nowSeconds();
    if(!autorotating && now-lastInteraction>AUTOROTATE_DELAY)
        autorotating=1;
    static double lastF=0;
    if(autorotating){
        if(lastF==0) lastF=now;
        double dt=now-lastF;
        yaw+=AUTOROTATE_SPEED*dt;
        if(yaw<0) yaw+=360; else if(yaw>=360) yaw-=360;
        glutPostRedisplay();
        lastF=now;
    } else lastF=0;
}

int main(int argc,char**argv){
    if(argc<2){
        fprintf(stderr,"Usage: %s <panorama.jpg/png> | <cubemap_folder>\n",argv[0]);
        return 1;
    }
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA|GLUT_DEPTH);
    glutInitWindowSize(winW,winH);
    glutCreateWindow("Chasm The Rift Remastered Cubemap/Skybox Viewer v1.0.0. By SMR9000");

    glEnable(GL_DEPTH_TEST);
    glClearColor(.1f,.1f,.1f,1);

    // load textures
    const char*arg=argv[1];
    size_t L=strlen(arg);
    if((L>4&&(strstr(arg+L-4,".jpg")||strstr(arg+L-4,".png")))||
       (L>5&&strstr(arg+L-5,".jpeg"))){
        viewMode=MODE_PANORAMA;
        texPan=loadTexture(arg);
    } else {
        viewMode=MODE_CUBEMAP;
        const char*slash=strrchr(arg,'/');
        #ifdef _WIN32
        if(!slash) slash=strrchr(arg,'\\');
        #endif
        const char*name=slash?slash+1:arg;
        char base[256]; snprintf(base,256,"%s.cel.",name);
        texCube=loadCubemap(arg,base,".png");
    }

    lastInteraction=nowSeconds();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutIdleFunc(idle);
    glutMainLoop();
    return 0;
}
