// viewer102.c – v1.2.0 by SMR9000
// .3O + optional .ANI viewer with textured rendering,
// front-side default view, vertical‐flip–fixed UVs, dynamic lighting,
// per‐poly translucency (bit2=80% translucent @20% opacity “Very Translucent”,
// bit3=40% translucent @60% opacity “Half Translucent”),
// WSAD/arrow + mouse drag, wireframe toggle,
// palette BG (auto dominant), play/pause,
// Shading (off by default, F6), interpolation on by default,
// bilinear/nearest filter toggle (F5),
// texture preview (T) top‐right rotated,
// filter modes (1=bit2‐only, 2=bit3‐only, 3=bit0‐only, 0=all),
// R: reset EVERYTHING (including zoom, angles, pan),
// ESC to quit, F1 toggles help,
// bottom info text for bit properties,
// GLUT_BITMAP_HELVETICA_10 font,
// camera defaults zoomed‐in & lowered,
// model rotated about its own center,
// supports loading only .3O (no .ANI).
// Usage & compile on WSL mingw-w64:
//   x86_64-w64-mingw32-gcc -std=c99 -O2 \
//     -I./ -L./lib \
//     -o viewer18.exe viewer18.c \
//     -lfreeglut -lopengl32 -lglu32 -lwinmm
// • Bottom‐left red arrow exactly aligned with text baseline
// • Top‐left controls each on its own line
// • F1 toggles all on‐screen text overlays
// • All prior functionality retained
// • x86_64-w64-mingw32-gcc -std=c99 -O2 -I./ -L./lib -o 3oviewer.exe viewer120.c -lfreeglut -lopengl32 -lglu32 -lwinmm

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <math.h>
#include <GL/freeglut.h>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE GL_CLAMP
#endif
#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef max
#define max(a,b) (((a)>(b))?(a):(b))
#endif

#define OFF_POLY   0x0000
#define OFF_VERT   0x3200
#define OFF_VCNT   0x4800
#define OFF_PCNT   0x4802
#define OFF_SKH    0x4804
#define OFF_SKIN   0x4806
#define SKIN_W     64
static const float SCALE3O = 1.0f/2048.0f;

#pragma pack(push,1)
typedef struct {
    uint16_t vi[4];
    uint16_t uv[4][2];
    uint8_t  pad[4];
    uint8_t  group, flags;
    int16_t  uv_off;
} POLY;
typedef struct { int16_t x,y,z; } VERT;
#pragma pack(pop)

// Raw buffers
static uint8_t *raw3o = NULL, *rawAni = NULL;
static size_t   size3o, sizeAni;

// Palette & texture
static uint8_t  palette[256][3];
static GLuint   texID;
static uint16_t skinH;
static size_t   skinPixels;

// Mesh & animation
static POLY    *polys     = NULL;
static VERT    *baseVerts = NULL;
static uint16_t vcount, pcount;
static VERT    *animVerts = NULL;
static int      totalFrames = 0;

// Model center
static float centerX, centerY, centerZ;

// View state
static bool playing      = true;
static bool doCull       = false;
static bool shading      = false;
static bool wireframe    = false;
static bool interpFrames = true;
static bool showTexPrev  = false;
static bool useLinear    = false;
// Toggle all text overlays
static bool showText     = true;

static float zoom   = 1.0f;
static float angleY = 0, angleX = 0;
static float panX   = 0, panY   = 0.05f;

static int bgIndex        = 0;
static int defaultBgIndex = 0;
static int winW = 800, winH = 600;

static int lastT = 0;
static float accTime = 0, frameDur = 0.1f;
static int curFrame = 0;

// Bit‐filter: -1 = no filter, 0–7 = show only polys with that bit
static int filterBit = -1;

// Mouse
static bool mouseDown = false;
static int  lastMouseX, lastMouseY;
static const float MOUSE_SENS = 0.3f;

// Helpers
static int clampi(int x,int lo,int hi){ return x<lo?lo:(x>hi?hi:x); }
static void drawText(const char *s,int x,int y){
    glRasterPos2i(x,y);
    while(*s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10,*s++);
}
static void computeNormal(const float a[3],const float b[3],const float c[3], float n[3]){
    float ux=b[0]-a[0], uy=b[1]-a[1], uz=b[2]-a[2];
    float vx=c[0]-a[0], vy=c[1]-a[1], vz=c[2]-a[2];
    n[0]=uy*vz-uz*vy; n[1]=uz*vx-ux*vz; n[2]=ux*vy-uy*vx;
    float L=sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
    if(L>0){ n[0]/=L; n[1]/=L; n[2]/=L; }
}
static inline bool skipPoly(uint8_t flags, bool needTrans){
    if(filterBit>=0 && !(flags & (1<<filterBit))) return true;
    bool t2 = (flags & 4)!=0;
    bool t3 = (flags & 8)!=0;
    bool isT = t2||t3;
    return needTrans ? !isT : isT;
}

// Load palette (.act)
static void loadPalette(const char *fn){
    FILE *f = fopen(fn,"rb"); if(!f){ perror(fn); exit(1); }
    fseek(f,0,SEEK_END); long sz = ftell(f);
    fseek(f,sz-768,SEEK_SET);
    fread(palette,1,768,f);
    fclose(f);
}
// Update texture filtering
static void updateFilter(){
    glBindTexture(GL_TEXTURE_2D, texID);
    GLint f = useLinear ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,f);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,f);
}

// Load .3O mesh + skin
static void load3O(const char *fn){
    FILE *f = fopen(fn,"rb"); if(!f){ perror(fn); exit(1); }
    fseek(f,0,SEEK_END); size3o = ftell(f); fseek(f,0,SEEK_SET);
    raw3o = malloc(size3o); fread(raw3o,1,size3o,f); fclose(f);

    vcount = *(uint16_t*)(raw3o + OFF_VCNT);
    pcount = *(uint16_t*)(raw3o + OFF_PCNT);
    skinH  = *(uint16_t*)(raw3o + OFF_SKH);
    skinPixels = SKIN_W * skinH;

    // Compute center
    VERT *vv = (VERT*)(raw3o + OFF_VERT);
    int16_t mnx=INT16_MAX, mxx=INT16_MIN,
            mny=INT16_MAX, mxy=INT16_MIN,
            mnz=INT16_MAX, mxz=INT16_MIN;
    for(int i=0;i<vcount;i++){
        mnx=min(mnx,vv[i].x); mxx=max(mxx,vv[i].x);
        mny=min(mny,vv[i].y); mxy=max(mxy,vv[i].y);
        mnz=min(mnz,vv[i].z); mxz=max(mxz,vv[i].z);
    }
    centerX = (mnx + mxx)*0.5f;
    centerY = (mny + mxy)*0.5f;
    centerZ = (mnz + mxz)*0.5f;

    // Dominant BG color
    int hist[256] = {0};
    uint8_t *skin = raw3o + OFF_SKIN;
    for(size_t i=0;i<skinPixels;i++){
        hist[skin[i]]++;
    }
    bgIndex = 0;
    for(int i=1;i<256;i++) if(hist[i]>hist[bgIndex]) bgIndex = i;
    defaultBgIndex = bgIndex;

    // Build RGBA skin texture
    uint8_t *rgba = malloc(skinPixels*4);
    for(size_t i=0;i<skinPixels;i++){
        uint8_t c = skin[i];
        rgba[4*i+0] = palette[c][0];
        rgba[4*i+1] = palette[c][1];
        rgba[4*i+2] = palette[c][2];
        rgba[4*i+3] = (c==4?0:255);
    }
    glGenTextures(1,&texID);
    glBindTexture(GL_TEXTURE_2D,texID);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,SKIN_W,skinH,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
    free(rgba);

    polys     = (POLY*)(raw3o + OFF_POLY);
    baseVerts = (VERT*)(raw3o + OFF_VERT);
    updateFilter();
}

// Load .ANI animation
static void loadANI(const char *fn){
    FILE *f = fopen(fn,"rb"); if(!f){ perror(fn); exit(1); }
    fseek(f,0,SEEK_END); sizeAni = ftell(f); fseek(f,0,SEEK_SET);
    rawAni = malloc(sizeAni); fread(rawAni,1,sizeAni,f); fclose(f);
    size_t off = (*(uint16_t*)rawAni == vcount) ? 2 : 0;
    totalFrames = (sizeAni - off) / (sizeof(VERT) * vcount);
    animVerts   = (VERT*)(rawAni + off);
}

static void display(){
    // Clear
    glClearColor(
      palette[bgIndex][0]/255.0f,
      palette[bgIndex][1]/255.0f,
      palette[bgIndex][2]/255.0f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    // Bit counts
    int cnt[8] = {0};
    for(int i=0;i<pcount;i++){
        uint8_t f = polys[i].flags;
        for(int b=0;b<8;b++) if(f&(1<<b)) cnt[b]++;
    }

    // Camera
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(60.0, winW/(float)winH, 0.1, 10.0);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    gluLookAt(panX, panY, -zoom, panX, panY, 0, 0,1,0);
    glRotatef(angleY,0,1,0); glRotatef(angleX,1,0,0);

    // Lighting & culling
    if(shading){
        glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_NORMALIZE);
        glLightfv(GL_LIGHT0,GL_POSITION,(float[]){-1,1,-1,0});
        glLightfv(GL_LIGHT0,GL_DIFFUSE,(float[]){1,1,1,1});
        glEnable(GL_COLOR_MATERIAL);
    } else {
        glDisable(GL_LIGHTING);
    }
    if(doCull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);

    // Bind texture
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texID);

    // Interpolate frames
    int f0 = totalFrames ? curFrame % totalFrames : 0;
    int f1 = totalFrames ? (f0+1) % totalFrames : 0;
    float alpha = (playing && interpFrames && totalFrames)
                  ? accTime/frameDur : 0;
    VERT *v0 = totalFrames ? animVerts + f0*vcount : baseVerts;
    VERT *v1 = totalFrames ? animVerts + f1*vcount : baseVerts;

    // Two passes
    for(int pass=0; pass<2; pass++){
        bool isTrans = (pass==1);
        if(isTrans){
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glDisable(GL_BLEND);
        }
        glPolygonMode(GL_FRONT_AND_BACK, wireframe?GL_LINE:GL_FILL);

        glBegin(GL_TRIANGLES);
        for(int i=0;i<pcount;i++){
            POLY *P = &polys[i];
            uint8_t f = P->flags;
            if(skipPoly(f, isTrans)) continue;

            if(isTrans){
                float a = (f & 4) ? 0.2f : 0.6f;
                glColor4f(1,1,1,a);
            } else {
                glColor4f(1,1,1,1);
            }

            float A[3],B[3],C[3],n[3];
            for(int k=0;k<3;k++){
                VERT *va=&v0[P->vi[k]], *vb=&v1[P->vi[k]];
                float x=(1-alpha)*(va->x-centerX)+alpha*(vb->x-centerX);
                float y=(1-alpha)*(va->y-centerY)+alpha*(vb->y-centerY);
                float z=(1-alpha)*(va->z-centerZ)+alpha*(vb->z-centerZ);
                float vx=x*SCALE3O, vy=z*SCALE3O, vz=y*SCALE3O;
                if(k==0){A[0]=vx;A[1]=vy;A[2]=vz;}
                if(k==1){B[0]=vx;B[1]=vy;B[2]=vz;}
                if(k==2){C[0]=vx;C[1]=vy;C[2]=vz;}
            }
            computeNormal(A,B,C,n); glNormal3fv(n);
            for(int k=0;k<3;k++){
                float u = P->uv[k][0]/(float)SKIN_W;
                int vt = clampi(P->uv[k][1]+P->uv_off,0,skinH-1);
                glTexCoord2f(u, vt/(float)skinH);
                glVertex3fv(k==0?A:(k==1?B:C));
            }
            // Quad second triangle
            if(P->vi[3]<vcount){
                float D[3],E[3],Fv[3],n2[3];
                int idx[3]={2,3,0};
                for(int j=0;j<3;j++){
                    VERT *va=&v0[P->vi[idx[j]]], *vb=&v1[P->vi[idx[j]]];
                    float x=(1-alpha)*(va->x-centerX)+alpha*(vb->x-centerX);
                    float y=(1-alpha)*(va->y-centerY)+alpha*(vb->y-centerY);
                    float z=(1-alpha)*(va->z-centerZ)+alpha*(vb->z-centerZ);
                    float vx=x*SCALE3O, vy=z*SCALE3O, vz=y*SCALE3O;
                    if(j==0){D[0]=vx;D[1]=vy;D[2]=vz;}
                    if(j==1){E[0]=vx;E[1]=vy;E[2]=vz;}
                    if(j==2){Fv[0]=vx;Fv[1]=vy;Fv[2]=vz;}
                }
                computeNormal(D,E,Fv,n2); glNormal3fv(n2);
                for(int j=0;j<3;j++){
                    float u = P->uv[idx[j]][0]/(float)SKIN_W;
                    int vt = clampi(P->uv[idx[j]][1]+P->uv_off,0,skinH-1);
                    glTexCoord2f(u, vt/(float)skinH);
                    glVertex3fv(j==0?D:(j==1?E:Fv));
                }
            }
        }
        glEnd();
    }
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);

    // Texture preview
    if(showTexPrev){
        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
        gluOrtho2D(0,winW,0,winH);
        glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();
        glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,texID);
        float x0=winW-SKIN_W, y0=winH-skinH, x1=winW, y1=winH;
        glBegin(GL_QUADS);
          glTexCoord2f(0,1); glVertex2f(x0,y0);
          glTexCoord2f(1,1); glVertex2f(x1,y0);
          glTexCoord2f(1,0); glVertex2f(x1,y1);
          glTexCoord2f(0,0); glVertex2f(x0,y1);
        glEnd();
        glDisable(GL_TEXTURE_2D);
        glMatrixMode(GL_PROJECTION); glPopMatrix();
        glMatrixMode(GL_MODELVIEW);  glPopMatrix();
    }

    // Top-left controls & frame counter, one per line
    if(showText){
        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
        gluOrtho2D(0,winW,0,winH);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
        int y = winH - 16;
        drawText("Mouse drag: Rotate",10,y); y-=14;
        drawText("Tab: Wireframe",10,y); y-=14;
        drawText("W/S: Zoom",10,y); y-=14;
        drawText("A/D: Rotate",10,y); y-=14;
        drawText("Arrows: Pan",10,y); y-=14;
        drawText("PgUp / PgDn: BG color",10,y); y-=14;
        drawText("Space: Play/Pause",10,y); y-=14;
        drawText("+ / -: Step Frame",10,y); y-=14;
        drawText("F5: Toggle Filter",10,y); y-=14;
        drawText("F6: Toggle Shading",10,y); y-=14;
        drawText("F7: Toggle Interpolation",10,y); y-=14;
        drawText("0 to 7: Filter by Bit",10,y); y-=14;
        drawText("T: Texture Preview",10,y); y-=14;
        drawText("R: Reset All",10,y); y-=14;
        drawText("F1: Toggle Text",10,y); y-=14;
        drawText("ESC: Quit",10,y); y-=14;
        // Frame counter
        {
          char fb[32];
          int df = totalFrames ? (curFrame % totalFrames) + 1 : 0;
          sprintf(fb,"Frame: %d/%d", df, totalFrames);
          drawText(fb,10,y);
        }
        glMatrixMode(GL_PROJECTION); glPopMatrix();
        glMatrixMode(GL_MODELVIEW);  glPopMatrix();
    }

    // Bottom-left panel + perfectly aligned red arrow
    if(showText){
        static const char* bitDesc[8] = {
          "Double-Sided","AlphaTested",
          "Very Translucent","Half Translucent",
          "Unused/Reserved","Invisible(DOS)","Invisible(DOS)","Invisible(DOS)"
        };
        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
        gluOrtho2D(0,winW,0,winH);
        glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();
        int y = 10;
        char buf[128];
        // 3O stats
        sprintf(buf,"3O: verts=%u  polys=%u  skinH=%u",vcount,pcount,skinH);
        drawText(buf,10,y); y+=14;
        // ANI stats
        sprintf(buf,"ANI: totalFrames=%d",totalFrames);
        drawText(buf,10,y); y+=14;
        // Bits
        for(int b=0;b<8;b++){
            if(filterBit==b){
                // arrow vertically centered on text line
                glColor3f(1,0,0);
                glBegin(GL_TRIANGLES);
                  glVertex2i(4,  y+1);
                  glVertex2i(4,  y+11);
                  glVertex2i(10, y+6);
                glEnd();
                glColor3f(1,1,1);
            }
            sprintf(buf,"Bit %d (%s): %d", b, bitDesc[b], cnt[b]);
            drawText(buf,10,y);
            y+=14;
        }
        glMatrixMode(GL_PROJECTION); glPopMatrix();
        glMatrixMode(GL_MODELVIEW);  glPopMatrix();
    }

    glutSwapBuffers();
}

static void idle(){
    int now = glutGet(GLUT_ELAPSED_TIME);
    if(!lastT) lastT = now;
    int dt = now - lastT; lastT = now;
    if(playing && totalFrames>0){
        accTime += dt * 0.001f;
        if(accTime >= frameDur){
            accTime -= frameDur;
            curFrame++;
        }
    }
    glutPostRedisplay();
}

static void reshape(int w,int h){
    winW = w; winH = h;
    glViewport(0,0,w,h);
}

static void mouse(int b,int s,int x,int y){
    if(b==GLUT_LEFT_BUTTON){
        mouseDown = (s==GLUT_DOWN);
        lastMouseX = x; lastMouseY = y;
    }
}
static void motion(int x,int y){
    if(mouseDown){
        angleY += (x - lastMouseX) * MOUSE_SENS;
        angleX += (y - lastMouseY) * MOUSE_SENS;
        lastMouseX = x; lastMouseY = y;
    }
}

static void keyboard(unsigned char k,int x,int y){
    (void)x;(void)y;
    if(k>='0'&&k<='7'){
        int b=k-'0';
        filterBit=(filterBit==b?-1:b);
        return;
    }
    switch(k){
      case 27: exit(0);
      case '\t': wireframe=!wireframe; break;
      case 'w': zoom=zoom>0.2f?zoom-0.2f:zoom; break;
      case 's': zoom+=0.2f; break;
      case 'a': angleY-=5; break;
      case 'd': angleY+=5; break;
      case ' ': playing=!playing; break;
      case 'T': case 't': showTexPrev=!showTexPrev; break;
      case 'R': case 'r':
        playing=true; doCull=false; shading=false; wireframe=false;
        interpFrames=true; showTexPrev=false; useLinear=false;
        zoom=1; angleY=0; angleX=0; panX=0; panY=0.05f;
        curFrame=0; accTime=0; bgIndex=defaultBgIndex; filterBit=-1;
        updateFilter();
        break;
      case '+':
        curFrame = totalFrames?(curFrame+1)%totalFrames:0;
        playing=false; break;
      case '-':
        curFrame = totalFrames?(curFrame-1+totalFrames)%totalFrames:0;
        playing=false; break;
    }
}

static void special(int k,int x,int y){
    (void)x;(void)y;
    switch(k){
      case GLUT_KEY_F1: showText=!showText; break;
      case GLUT_KEY_F4: doCull=!doCull; break;
      case GLUT_KEY_F5: useLinear=!useLinear; updateFilter(); break;
      case GLUT_KEY_F6: shading=!shading; break;
      case GLUT_KEY_F7: interpFrames=!interpFrames; break;
      case GLUT_KEY_PAGE_UP:   bgIndex=(bgIndex+1)&0xFF; break;
      case GLUT_KEY_PAGE_DOWN: bgIndex=(bgIndex-1)&0xFF; break;
      case GLUT_KEY_LEFT:  panX-=0.1f; break;
      case GLUT_KEY_RIGHT: panX+=0.1f; break;
      case GLUT_KEY_UP:    panY-=0.1f; break;
      case GLUT_KEY_DOWN:  panY+=0.1f; break;
    }
}

int main(int argc,char**argv){
    if(argc<2||argc>3){
        fprintf(stderr,"Usage: %s <model.3o> [model.ani]\n",argv[0]);
        return 1;
    }
    loadPalette("chasmpalette.act");
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA|GLUT_DEPTH);
    glutInitWindowSize(winW,winH);
    glutCreateWindow("Chasm The Rift 3O+ANI Viewer v1.2.0 by SMR9000");
    glEnable(GL_DEPTH_TEST);
    load3O(argv[1]);
    if(argc==3) loadANI(argv[2]);
    glutDisplayFunc(display);
    glutIdleFunc(idle);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMainLoop();
    return 0;
}
