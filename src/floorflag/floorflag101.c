// Chasm Floor Tag Editor v1.0.1 (105)
// Compile: x86_64-w64-mingw32-gcc floorflag.c -o floorflag.exe -I. -L./lib -l:libfreeglut.a -lopengl32 -lgdi32 -std=c99 -static floorflag.res
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"
#include <GL/freeglut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_FLOORS       64
#define TEX_SIZE         64
#define HEADER_SIZE      64
#define UNKNOWN_BYTES    64
#define MIN_WINDOW_W     768
#define MIN_WINDOW_H     512
#define DEFAULT_WINDOW_W 768
#define DEFAULT_WINDOW_H 512

static const char *g_filename;
static unsigned char *fileBuf = NULL;
static long fileSize = 0;
static unsigned char floorIdx[MAX_FLOORS][TEX_SIZE*TEX_SIZE];
static unsigned char *floorRGBA[MAX_FLOORS];
static GLuint texBase[MAX_FLOORS];
static unsigned char palette[256][3];
static int tileFlags[MAX_FLOORS] = {0};
static int windowW = DEFAULT_WINDOW_W, windowH = DEFAULT_WINDOW_H;
static int viewportX, viewportY, viewportW, viewportH;
static bool dirty = false;

typedef struct { int x,y; } Point;
static Point sel = {0,0};
static bool showFlagDetails = false;

static void buildBase(char *out){
    const char *fn=strrchr(g_filename,'/')?strrchr(g_filename,'/')+1:g_filename;
    fn=strrchr(fn,'\\')?strrchr(fn,'\\')+1:fn;
    int j=0;
    for(int i=0;fn[i]&&j<255;i++) if(fn[i]!='.') out[j++]=fn[i];
    out[j]='\0';
}

void loadPalette(const char *path){
    FILE *f=fopen(path,"rb"); 
    if(!f){fprintf(stderr,"No palette\n");exit(1);}
    for(int i=0;i<256;i++) fread(palette[i],1,3,f);
    fclose(f);
}

void loadFloors(const char *path){
    FILE *f=fopen(path,"rb"); 
    if(!f){fprintf(stderr,"Open %s failed\n",path);exit(1);}
    fseek(f,0,SEEK_END); fileSize=ftell(f); fseek(f,0,SEEK_SET);
    fileBuf=malloc(fileSize); fread(fileBuf,1,fileSize,f); fclose(f);

    for(int i=0;i<MAX_FLOORS;i++) tileFlags[i] = fileBuf[i];
    unsigned char *ptr=fileBuf+HEADER_SIZE;
    for(int i=0;i<MAX_FLOORS;i++){
        floorRGBA[i]=malloc(TEX_SIZE*TEX_SIZE*4);
        memcpy(floorIdx[i],ptr,TEX_SIZE*TEX_SIZE);
        for(int p=0;p<TEX_SIZE*TEX_SIZE;p++){
            unsigned char ci=floorIdx[i][p];
            floorRGBA[i][4*p+0]=palette[ci][0];
            floorRGBA[i][4*p+1]=palette[ci][1];
            floorRGBA[i][4*p+2]=palette[ci][2];
            floorRGBA[i][4*p+3]=255;
        }
        ptr+=TEX_SIZE*TEX_SIZE+(TEX_SIZE/2)*(TEX_SIZE/2)+(TEX_SIZE/4)*(TEX_SIZE/4)+(TEX_SIZE/8)*(TEX_SIZE/8)+UNKNOWN_BYTES;
    }
}

void setClearColorFromPalette(){
    glClearColor(0.28,0.345,0.345,1);
}

void initGL(){
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
    glGenTextures(MAX_FLOORS,texBase);
    for(int i=0;i<MAX_FLOORS;i++){
        glBindTexture(GL_TEXTURE_2D,texBase[i]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,floorRGBA[i]);
    }
    setClearColorFromPalette();
    viewportW = 512; viewportH = 512;
    viewportX = windowW - 512;
    viewportY = (windowH > 512) ? (windowH-512)/2 : 0;
}

void drawText(const char *s,int x,int y){
    glRasterPos2i(x,y);
    while(*s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10,*s++);
}

void saveFlagsJSON(){
    char base[256]; buildBase(base);
    char out[256]; sprintf(out, "%s_flags.json", base);
    FILE *f = fopen(out, "w");
    if (!f) { fprintf(stderr, "Could not write %s\n", out); return; }
    fprintf(f, "{\n  \"tile_flags\": [\n");
    for(int i=0;i<MAX_FLOORS;i++) {
        unsigned char flag = tileFlags[i];
        fprintf(f, "    {\"tile\":%d, \"flag_dec\":%d, \"flag_hex\":\"0x%02X\", \"flag_bits\":\"", i, flag, flag);
        for(int b=7;b>=0;b--) fputc( (flag&(1<<b))?'1':'0', f);
        fprintf(f, "\"}%s\n", (i<MAX_FLOORS-1)?",":"");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
    printf("Saved flags as %s\n", out);
}

void importFlagsJSON(){
    char base[256]; buildBase(base);
    char fn[256]; sprintf(fn, "%s_flags.json", base);
    FILE *f = fopen(fn, "r");
    if (!f) { fprintf(stderr, "Could not open %s\n", fn); return; }
    char line[256]; int count=0;
    while(fgets(line,sizeof(line),f)){
        char *p = strstr(line,"\"tile\":");
        if (p && sscanf(p,"\"tile\":%d, \"flag_dec\":%d", (int[]){0}, (int[]){0})==2) {
            int tile, flag;
            sscanf(p,"\"tile\":%d, \"flag_dec\":%d", &tile, &flag);
            if(tile>=0 && tile<MAX_FLOORS) {
                tileFlags[tile]=(unsigned char)flag;
                count++;
            }
        }
    }
    fclose(f);
    if (count) {
        dirty = true;
        printf("Imported %d flags from %s\n", count, fn);
        glutPostRedisplay();
    } else printf("No flags imported from %s\n", fn);
}

void saveFloorsFile(){
    for(int i=0;i<MAX_FLOORS;i++) fileBuf[i] = (unsigned char)tileFlags[i];
    FILE *f = fopen(g_filename, "wb");
    if (!f) { perror("save"); return; }
    fwrite(fileBuf,1,fileSize,f); fclose(f);
    dirty = false;
    printf("Saved flags to %s\n", g_filename);
}

void renderOverlay(){
    glViewport(0,0,windowW,windowH);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0,windowW,0,windowH,-1,1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glColor3f(palette[63][0]/255.0f, palette[63][1]/255.0f, palette[63][2]/255.0f);
    char buf[128]; int xbase = 10, ybase = windowH-20, dy=15;

    drawText("FILE INFO",xbase,ybase); ybase -= dy;
    const char *fn=strrchr(g_filename,'/')?strrchr(g_filename,'/')+1:g_filename;
    fn=strrchr(fn,'\\')?strrchr(fn,'\\')+1:fn;
    sprintf(buf,"File: %s",fn); drawText(buf,xbase,ybase); ybase -= dy;
    sprintf(buf,"Tiles: %d",MAX_FLOORS); drawText(buf,xbase,ybase); ybase -= dy;
    sprintf(buf,"Selected: %d",sel.y*8+sel.x); drawText(buf,xbase,ybase); ybase -= dy;
    ybase -= 5;

    drawText("FLAGS INFO",xbase,ybase); ybase -= dy;
    int flag = tileFlags[sel.y*8+sel.x];
    sprintf(buf,"Flag (dec): %d", flag); drawText(buf,xbase,ybase); ybase -= dy;
    sprintf(buf,"Flag (hex): 0x%02X", flag); drawText(buf,xbase,ybase); ybase -= dy;
    char bits[16]; for(int i=7;i>=0;i--) bits[7-i] = (flag&(1<<i)) ? '1' : '0'; bits[8]=0;
    sprintf(buf,"Flag (bits): %s", bits); drawText(buf,xbase,ybase); ybase -= dy;
    if (showFlagDetails) {
        char labels[] = "Flags: 7 6 5 4 3 2 1 0 = ";
        char vals[16];
        for(int i=7;i>=0;i--) vals[7-i]=bits[i]; vals[8]=0;
        char info[128]; sprintf(info,"%s%s",labels,vals);
        drawText(info,xbase,ybase); ybase -= dy;
    }
    ybase -= 5;

    drawText("CONTROLS",xbase,ybase); ybase -= dy;
    drawText("Arrows/Mouse: Select tile",xbase,ybase); ybase -= dy;
    drawText("Right Click: Edit tile value",xbase,ybase); ybase -= dy;
    drawText("1-9,0: Set selected tile",xbase,ybase); ybase -= dy;
    drawText("F1-F9: Set ALL tiles to 1-9",xbase,ybase); ybase -= dy;
    drawText("F10: Set ALL tiles to 0",xbase,ybase); ybase -= dy;
    drawText("S: Save JSON",xbase,ybase); ybase -= dy;
    drawText("I: Import JSON",xbase,ybase); ybase -= dy;
    drawText("F12: Save to game file",xbase,ybase); ybase -= dy;
    drawText("ESC: Quit",xbase,ybase); ybase -= dy;
    ybase -= 5;

    if (dirty) drawText("MODIFIED - PRESS F12 TO SAVE!",xbase,20);
    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
}

void display(){
    glViewport(0,0,windowW,windowH);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(viewportX,viewportY,viewportW,viewportH);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    float gridNDC = 2.0f, half=gridNDC*0.5f, step=gridNDC/8.0f;
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
    glColor3f(palette[63][0]/255.0f, palette[63][1]/255.0f, palette[63][2]/255.0f);
    glLineWidth(1); glBegin(GL_LINE_LOOP);
      float x0=-half+sel.x*step, y0=half-sel.y*step;
      glVertex2f(x0,y0); glVertex2f(x0+step,y0);
      glVertex2f(x0+step,y0-step); glVertex2f(x0,y0-step);
    glEnd();
    renderOverlay();
    glutSwapBuffers();
}

void reshape(int w,int h){
    glutReshapeWindow(DEFAULT_WINDOW_W, DEFAULT_WINDOW_H);
}

void onSpecialKey(int k,int x,int y){
    switch(k){
        case GLUT_KEY_LEFT:  sel.x=(sel.x+7)%8; break;
        case GLUT_KEY_RIGHT: sel.x=(sel.x+1)%8; break;
        case GLUT_KEY_UP:    sel.y=(sel.y+7)%8; break;
        case GLUT_KEY_DOWN:  sel.y=(sel.y+1)%8; break;
        case GLUT_KEY_F1: case GLUT_KEY_F2: case GLUT_KEY_F3: case GLUT_KEY_F4: 
        case GLUT_KEY_F5: case GLUT_KEY_F6: case GLUT_KEY_F7: case GLUT_KEY_F8: 
        case GLUT_KEY_F9: {
            int value = k - GLUT_KEY_F1 + 1;
            for(int i=0;i<MAX_FLOORS;i++) tileFlags[i] = value;
            dirty = true;
            break;
        }
        case GLUT_KEY_F10:
            for(int i=0;i<MAX_FLOORS;i++) tileFlags[i] = 0;
            dirty = true;
            break;
        case GLUT_KEY_F12:
            if (dirty) saveFloorsFile();
            break;
        default: return;
    }
    showFlagDetails = false;
    glutPostRedisplay();
}

void onAsciiKey(unsigned char key,int x,int y){
    switch(key){
        case 27: exit(0);
        case 's': case 'S': saveFlagsJSON(); break;
        case 'i': case 'I': importFlagsJSON(); break;
        default:
            if(key >= '0' && key <= '9'){
                int value = (key == '0') ? 0 : key - '0';
                tileFlags[sel.y*8 + sel.x] = value;
                dirty = true;
                glutPostRedisplay();
            }
    }
}

void onMouse(int b,int s,int mx,int my){
    if(s==GLUT_DOWN){
        int ry=windowH-1-my;
        float ox=viewportX, oy=viewportY, rp=512;
        if(mx>=ox && mx<ox+rp && ry>=oy && ry<oy+rp){
            float rx=(mx-ox)/rp, ryf=(ry-oy)/rp;
            int tx=(int)(rx*8), ty=7-(int)(ryf*8);
            if(tx>=0&&tx<8&&ty>=0&&ty<8){
                sel.x=tx; sel.y=ty;
                if(b==GLUT_LEFT_BUTTON) showFlagDetails = true;
                else if(b==GLUT_RIGHT_BUTTON){
                    printf("Enter flag value (0-255) for tile %d: ", sel.y*8+sel.x);
                    int v; fflush(stdout);
                    if(scanf("%d",&v)==1 && v>=0 && v<=255){
                        tileFlags[sel.y*8+sel.x] = (unsigned char)v;
                        dirty=true;
                    }
                    while(getchar()!='\n'); // Clear input buffer
                }
            }
        }
    }
    glutPostRedisplay();
}

int main(int argc,char **argv){
    g_filename=(argc>1)?argv[1]:"FLOORS.XX";
    loadPalette("chasmpalette.act");
    loadFloors(g_filename);

    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA);
    glutInitWindowSize(windowW,windowH);
    glutCreateWindow("Chasm The Rift Floor Flaf Editor v1.0.1 by SMR9000");

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutSpecialFunc(onSpecialKey);
    glutKeyboardFunc(onAsciiKey);
    glutMouseFunc(onMouse);

    initGL();
    glutMainLoop();
    return 0;
}