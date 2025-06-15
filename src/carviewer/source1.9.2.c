//x86_64-w64-mingw32-gcc source2.0FINAL.c -o carviewer.exe   -Iinclude -Llib -lfreeglut -lopengl32 -lglu32 -lwinmm   carviewer.res chasmpalette.o

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <GL/gl.h>
#include <GL/freeglut.h>

#ifdef _WIN32
  #include <windows.h>
  #include <mmsystem.h>
  #pragma comment(lib, "winmm.lib")
#endif

#define SCALE          (1.0f/2048.0f)
#define TEX_WIDTH      64
// Volume factor: 1.0 = full volume; 0.7 = 70% (30% reduction)
#define VOLUME_FACTOR  0.4f

#pragma pack(push,1)
typedef struct {
    uint16_t animations[20];
    uint16_t submodels_animations[3][2];
    uint16_t unknown0[9];
    uint16_t sounds[7];
    uint16_t unknown1[9];
} CARHeader;
typedef struct {
    uint16_t vertices_indices[4];
    uint16_t uv[4][2];
    uint8_t  unknown0[4];
    uint8_t  group_id, flags;
    uint16_t v_offset;
} CARPolygon;
typedef struct { int16_t xyz[3]; } Vertex;
#pragma pack(pop)

extern unsigned char _binary_chasmpalette_act_start[];
extern unsigned char _binary_chasmpalette_act_end[];

static uint8_t    *rawData       = NULL;
static size_t      rawSize       = 0;
static uint8_t     paletteRGB[256][3];
static uint8_t    *textureRGBA   = NULL;
static uint16_t    texWidth, texHeight;
static Vertex     *animationFrames = NULL;
static size_t      vertexCount   = 0, polygonCount = 0, frameCount = 0;
static float       animationTime = 0.0f, frameDuration = 0.1f;
static int         animating     = 0;

typedef struct { size_t start, count; } AnimInfo;
static AnimInfo   anims[20];
static int        animCount     = 0, currentAnim = 0;
static size_t     animFrameIdx  = 0;

static CARPolygon *polygons      = NULL;
static GLuint      texID;

static float bgColor[3] = {0.2f,0.2f,0.3f};
static int initBgPaletteIndex=0, currentBgPaletteIndex=0;

static float modelCenterX, modelCenterY, modelCenterZ;
static float initRotateX=-90, initRotateY=0, initTranslateX=0, initTranslateY=0, initZoom=2.5f;
static float rotateX=-90, rotateY=0, translateX=0, translateY=0, zoom=2.5f;
static int lastMouseX, lastMouseY, leftButtonDown=0;
static int wireframeMode=0, linearFiltering=0, spinning=1, overlayEnabled=1;
static int winWidth=800, winHeight=600;

// In-memory WAV buffers for banks 0–6
static uint8_t* wavBuffers[7] = { NULL };
static uint32_t wavBufferLens[7] = { 0 };

static int endswith(const char *s, const char *suffix) {
    size_t sl = strlen(s), su = strlen(suffix);
    return (sl>=su && strcasecmp(s+sl-su, suffix)==0);
}

void load_palette_embedded(void) {
    unsigned char *p = _binary_chasmpalette_act_start;
    size_t len = _binary_chasmpalette_act_end - p;
    if (len != 256*3) { fprintf(stderr,"Bad palette\n"); exit(1); }
    for (int i = 0; i < 256; i++) {
        paletteRGB[i][0]=p[3*i+0];
        paletteRGB[i][1]=p[3*i+1];
        paletteRGB[i][2]=p[3*i+2];
    }
}

static void stopSpin(){ spinning = 0; }

void load_car_model(const char *fn) {
    FILE *f = fopen(fn,"rb");
    if (!f) { perror(fn); exit(1); }
    fseek(f,0,SEEK_END); rawSize=ftell(f); fseek(f,0,SEEK_SET);
    rawData=malloc(rawSize); fread(rawData,1,rawSize,f); fclose(f);

    vertexCount  = *(uint16_t*)(rawData+0x4866);
    polygonCount = *(uint16_t*)(rawData+0x4868);
    uint16_t texels = *(uint16_t*)(rawData+0x486A);
    texWidth=TEX_WIDTH; texHeight=texels/TEX_WIDTH;

    size_t texOffset=0x486C;
    uint8_t *indices=rawData+texOffset;
    textureRGBA=malloc(texWidth*texHeight*4);
    for(size_t i=0;i<texWidth*texHeight;i++){
        uint8_t idx=indices[i];
        textureRGBA[4*i+0]=paletteRGB[idx][0];
        textureRGBA[4*i+1]=paletteRGB[idx][1];
        textureRGBA[4*i+2]=paletteRGB[idx][2];
        textureRGBA[4*i+3]=(paletteRGB[idx][0]==4 && paletteRGB[idx][1]==4 && paletteRGB[idx][2]==4)?0:255;
    }
    glGenTextures(1,&texID);
    glBindTexture(GL_TEXTURE_2D,texID);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,texWidth,texHeight,0,GL_RGBA,GL_UNSIGNED_BYTE,textureRGBA);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);

    uint8_t *fd = rawData + texOffset + texels;
    frameCount = (rawSize - (fd - rawData)) / (vertexCount*sizeof(Vertex));
    animationFrames = (Vertex*)fd;

    CARHeader *hdr=(CARHeader*)rawData;
    size_t off=0; animCount=0;
    for(int i=0;i<20;i++){
        uint16_t b=hdr->animations[i];
        if(b){
            size_t n=b/(vertexCount*sizeof(Vertex));
            anims[animCount].start=off;
            anims[animCount].count=n;
            off+=n; animCount++;
        }
    }
    if(!animCount){ anims[0].start=0; anims[0].count=frameCount; animCount=1; }
    currentAnim=0; animFrameIdx=0;

    polygons=(CARPolygon*)(rawData+0x66);

    // choose background color
    int counts[256]={0};
    for(size_t i=0;i<texWidth*texHeight;i++){
        uint8_t idx=indices[i];
        float b=(paletteRGB[idx][0]+paletteRGB[idx][1]+paletteRGB[idx][2])/(3.0f*255.0f);
        if(b>0.2f) counts[idx]++;
    }
    int best=0,bc=0;
    for(int i=0;i<256;i++) if(counts[i]>bc){ bc=counts[i]; best=i; }
    initBgPaletteIndex=currentBgPaletteIndex=best;
    bgColor[0]=paletteRGB[best][0]/255.0f;
    bgColor[1]=paletteRGB[best][1]/255.0f;
    bgColor[2]=paletteRGB[best][2]/255.0f;

    // center model
    float minX=1e9f,minY=1e9f,minZ=1e9f;
    float maxX=-1e9f,maxY=-1e9f,maxZ=-1e9f;
    for(size_t i=0;i<vertexCount;i++){
        float x=animationFrames[i].xyz[0]*SCALE;
        float y=animationFrames[i].xyz[1]*SCALE;
        float z=animationFrames[i].xyz[2]*SCALE;
        if(x<minX)minX=x; if(x>maxX)maxX=x;
        if(y<minY)minY=y; if(y>maxY)maxY=y;
        if(z<minZ)minZ=z; if(z>maxZ)maxZ=z;
    }
    modelCenterX=(minX+maxX)*0.5f;
    modelCenterY=(minY+maxY)*0.5f;
    modelCenterZ=(minZ+maxZ)*0.5f;

    // build WAV buffers and apply volume factor
    uint32_t totalBytes=0; for(int b=0;b<7;b++) totalBytes+=hdr->sounds[b];
    long audio_off=rawSize-totalBytes, pos=audio_off;
    for(int b=0;b<7;b++){
        uint16_t len=hdr->sounds[b];
        if(len){
            uint32_t ws=44+len;
            uint8_t *buf=malloc(ws);
            // header
            memcpy(buf+0,"RIFF",4);
            uint32_t chsz=36+len; memcpy(buf+4,&chsz,4);
            memcpy(buf+8,"WAVEfmt ",8);
            uint32_t sub1=16;    memcpy(buf+16,&sub1,4);
            uint16_t pcm=1,ch=1; memcpy(buf+20,&pcm,2); memcpy(buf+22,&ch,2);
            uint32_t rate=11025; memcpy(buf+24,&rate,4);
            uint32_t brate=rate*ch; memcpy(buf+28,&brate,4);
            uint16_t align=ch;   memcpy(buf+32,&align,2);
            uint16_t bps=8;      memcpy(buf+34,&bps,2);
            memcpy(buf+36,"data",4);
            uint32_t dlen=len;   memcpy(buf+40,&dlen,4);
            // copy and scale samples
            for(int i=0;i<len;i++){
                uint8_t s = rawData[pos+i];
                float centered = (float)s - 128.0f;
                centered *= VOLUME_FACTOR;
                int ns = (int)(centered + 128.0f);
                if(ns<0) ns=0; else if(ns>255) ns=255;
                buf[44+i] = (uint8_t)ns;
            }
            wavBuffers[b]=buf;
            wavBufferLens[b]=ws;
        }
        pos+=len;
    }
}

void idle(void){
    static int lt=0;
    int t=glutGet(GLUT_ELAPSED_TIME);
    float dt=(t-lt)/1000.0f; lt=t;
    if(spinning)rotateY+=0.2f;
    if(animating && anims[currentAnim].count>1){
        animationTime+=dt;
        if(animationTime>=frameDuration){
            animationTime-=frameDuration;
            animFrameIdx=(animFrameIdx+1)%anims[currentAnim].count;
        }
    }
    glutPostRedisplay();
}

void mouse(int btn,int st,int x,int y){
    stopSpin();
    if(btn==GLUT_LEFT_BUTTON){
        leftButtonDown = (st==GLUT_DOWN);
        lastMouseX = x; lastMouseY = y;
    }
}

void motion(int x,int y){
    stopSpin();
    if(leftButtonDown){
        rotateY += (x-lastMouseX)*0.5f;
        rotateX += (y-lastMouseY)*0.5f;
        lastMouseX = x; lastMouseY = y;
        glutPostRedisplay();
    }
}

void special(int key,int x,int y){
    stopSpin();
    switch(key){
      case GLUT_KEY_PAGE_UP:   currentBgPaletteIndex=(currentBgPaletteIndex+1)%256; break;
      case GLUT_KEY_PAGE_DOWN: currentBgPaletteIndex=(currentBgPaletteIndex+255)%256; break;
      case GLUT_KEY_F1:        overlayEnabled=!overlayEnabled; break;
      case GLUT_KEY_LEFT:      translateX-=0.1f; break;
      case GLUT_KEY_RIGHT:     translateX+=0.1f; break;
      case GLUT_KEY_UP:        translateY+=0.1f; break;
      case GLUT_KEY_DOWN:      translateY-=0.1f; break;
      case GLUT_KEY_F5:  if(wavBuffers[0]) PlaySound((LPCSTR)wavBuffers[0], NULL, SND_MEMORY|SND_ASYNC); break;
      case GLUT_KEY_F6:  if(wavBuffers[1]) PlaySound((LPCSTR)wavBuffers[1], NULL, SND_MEMORY|SND_ASYNC); break;
      case GLUT_KEY_F7:  if(wavBuffers[2]) PlaySound((LPCSTR)wavBuffers[2], NULL, SND_MEMORY|SND_ASYNC); break;
      case GLUT_KEY_F8:  if(wavBuffers[3]) PlaySound((LPCSTR)wavBuffers[3], NULL, SND_MEMORY|SND_ASYNC); break;
      case GLUT_KEY_F9:  if(wavBuffers[4]) PlaySound((LPCSTR)wavBuffers[4], NULL, SND_MEMORY|SND_ASYNC); break;
      case GLUT_KEY_F10: if(wavBuffers[5]) PlaySound((LPCSTR)wavBuffers[5], NULL, SND_MEMORY|SND_ASYNC); break;
      case GLUT_KEY_F11: if(wavBuffers[6]) PlaySound((LPCSTR)wavBuffers[6], NULL, SND_MEMORY|SND_ASYNC); break;
    }
    bgColor[0]=paletteRGB[currentBgPaletteIndex][0]/255.0f;
    bgColor[1]=paletteRGB[currentBgPaletteIndex][1]/255.0f;
    bgColor[2]=paletteRGB[currentBgPaletteIndex][2]/255.0f;
    glutPostRedisplay();
}

void keyboard(unsigned char k,int x,int y){
    stopSpin();
    switch(k){
      case 27: // ESC
        rotateX=initRotateX; rotateY=initRotateY;
        translateX=initTranslateX; translateY=initTranslateY;
        zoom=initZoom; spinning=1;
        currentBgPaletteIndex=initBgPaletteIndex;
        bgColor[0]=paletteRGB[initBgPaletteIndex][0]/255.0f;
        bgColor[1]=paletteRGB[initBgPaletteIndex][1]/255.0f;
        bgColor[2]=paletteRGB[initBgPaletteIndex][2]/255.0f;
        break;
      case 'w': zoom*=1.1f; break;
      case 's': zoom/=1.1f; break;
      case 'a': rotateY-=10; break;
      case 'd': rotateY+=10; break;
      case 'r': spinning=!spinning; break;
      case ' ': animating=!animating; break;
      case '\t':
        wireframeMode=!wireframeMode;
        glPolygonMode(GL_FRONT_AND_BACK, wireframeMode?GL_LINE:GL_FILL);
        break;
      case 'f':
        linearFiltering=!linearFiltering;
        glBindTexture(GL_TEXTURE_2D, texID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linearFiltering?GL_LINEAR:GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linearFiltering?GL_LINEAR:GL_NEAREST);
        break;
      case '1': if(animCount>=1){currentAnim=0;animFrameIdx=0;animationTime=0;} break;
      case '2': if(animCount>=2){currentAnim=1;animFrameIdx=0;animationTime=0;} break;
      case '3': if(animCount>=3){currentAnim=2;animFrameIdx=0;animationTime=0;} break;
      case '4': if(animCount>=4){currentAnim=3;animFrameIdx=0;animationTime=0;} break;
      case '5': if(animCount>=5){currentAnim=4;animFrameIdx=0;animationTime=0;} break;
      case '6': if(animCount>=6){currentAnim=5;animFrameIdx=0;animationTime=0;} break;
      case '7': if(animCount>=7){currentAnim=6;animFrameIdx=0;animationTime=0;} break;
      case '8': if(animCount>=8){currentAnim=7;animFrameIdx=0;animationTime=0;} break;
      case '9': if(animCount>=9){currentAnim=8;animFrameIdx=0;animationTime=0;} break;
      case '0': if(animCount>=10){currentAnim=9;animFrameIdx=0;animationTime=0;} break;
      case '+': case '=':
        if(animCount>0){currentAnim=(currentAnim+1)%animCount;animFrameIdx=0;animationTime=0;}
        break;
      case '-': case '_':
        if(animCount>0){currentAnim=(currentAnim+animCount-1)%animCount;animFrameIdx=0;animationTime=0;}
        break;
    }
    glutPostRedisplay();
}

void drawBitmapString(float x,float y,void*font,const char*s){
    glRasterPos2f(x,y);
    while(*s) glutBitmapCharacter(font,*s++);
}

void drawOverlay(){
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    gluOrtho2D(0,winWidth,0,winHeight);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glColor3f(0,0,0);
    const char* lines[]={
      "F1: Toggle Overlay","Space: Play/Pause","1-0: Select Anim","+/-: Cycle Anim",
      "R: Toggle Spin","ESC: Reset","W/S: Zoom","A/D: Rotate","TAB: Wireframe",
      "F: Filter","Arrows: Pan","PgUp/Dn: Change BG","Mouse Drag: Rotate","F5-F11: Play Sound"
    };
    for(int i=0;i<14;i++){
        drawBitmapString(10,winHeight-12*(i+1),GLUT_BITMAP_HELVETICA_12,lines[i]);
    }
    glEnable(GL_DEPTH_TEST);
    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
}

void display(void){
    glClearColor(bgColor[0],bgColor[1],bgColor[2],1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glColor3f(1,1,1);
    glLoadIdentity();
    glTranslatef(translateX,translateY,-5.0f/zoom);
    glTranslatef(modelCenterX,modelCenterY,modelCenterZ);
    glRotatef(rotateY,0,1,0);
    glRotatef(rotateX,1,0,0);
    glTranslatef(-modelCenterX,-modelCenterY,-modelCenterZ);
    float alpha=(animating && anims[currentAnim].count>1)?(animationTime/frameDuration):0.0f;
    size_t f0=anims[currentAnim].start+animFrameIdx;
    size_t f1=anims[currentAnim].start+((animFrameIdx+1)%anims[currentAnim].count);
    glBindTexture(GL_TEXTURE_2D,texID);
    glBegin(GL_TRIANGLES);
    for(size_t i=0;i<polygonCount;i++){
        CARPolygon *p=&polygons[i];
        for(int v=0;v<3;v++){
            int vi=p->vertices_indices[v];
            int16_t *pv0=animationFrames[f0*vertexCount+vi].xyz;
            int16_t *pv1=animationFrames[f1*vertexCount+vi].xyz;
            float x=(1-alpha)*pv0[0]+alpha*pv1[0];
            float y=(1-alpha)*pv0[1]+alpha*pv1[1];
            float z=(1-alpha)*pv0[2]+alpha*pv1[2];
            glTexCoord2f(p->uv[v][0]/(float)(texWidth<<8),(p->uv[v][1]+4*p->v_offset)/(float)(texHeight<<8));
            glVertex3f(x*SCALE,y*SCALE,z*SCALE);
        }
        if(p->vertices_indices[3]<(int)vertexCount){
            int ord[3]={0,2,3};
            for(int v=0;v<3;v++){
                int vi=p->vertices_indices[ord[v]];
                int16_t *pv0=animationFrames[f0*vertexCount+vi].xyz;
                int16_t *pv1=animationFrames[f1*vertexCount+vi].xyz;
                float x=(1-alpha)*pv0[0]+alpha*pv1[0];
                float y=(1-alpha)*pv0[1]+alpha*pv1[1];
                float z=(1-alpha)*pv0[2]+alpha*pv1[2];
                glTexCoord2f(p->uv[ord[v]][0]/(float)(texWidth<<8),(p->uv[ord[v]][1]+4*p->v_offset)/(float)(texHeight<<8));
                glVertex3f(x*SCALE,y*SCALE,z*SCALE);
            }
        }
    }
    glEnd();
    if(overlayEnabled) drawOverlay();
    glutSwapBuffers();
}

void reshape(int w,int h){
    winWidth=w; winHeight=h;
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(45.0f,(float)w/h,0.1f,100.0f);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc,char**argv){
    if(argc<2){
        fprintf(stderr,"Usage: %s <model.car>\n",argv[0]);
        return 1;
    }
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA|GLUT_DEPTH);
    glutInitWindowSize(winWidth,winHeight);
    glutCreateWindow("Chasm The Rift CAR Viewer v1.9.2 by SMR9000");
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    load_palette_embedded();
    load_car_model(argv[1]);

    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutSpecialFunc(special);
    glutKeyboardFunc(keyboard);
    glutIdleFunc(idle);
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);

    glutMainLoop();
    return 0;
}
