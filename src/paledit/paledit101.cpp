// Paledit101.cpp (v111)
// x86_64-w64-mingw32-g++ paledit111.cpp   -I. -ISTB -Llib -lfreeglut -lopengl32 -lglu32 -lm   -static-libstdc++ -static-libgcc   -o PALEDIT2.exe paledit.res

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "STB/stb_image_write.h"

#include <GL/freeglut.h>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <algorithm>

static uint8_t base_palette[256][3], disp_palette[256][3];
static bool    selected[256], changed[256];

static float   hue_shift   = 0.0f,
               saturation  = 1.0f,
               lightness   = 1.0f;

static const int PREV_W = 512, PREV_H = 512, TEXT_H = 64;
static const int WIN_W  = PREV_W, WIN_H  = PREV_H + TEXT_H;

//── Palette I/O ────────────────────────────────────────────────

void load_chasm(const char *fn){
    FILE *f=fopen(fn,"rb");
    if(!f){ perror(fn); exit(1); }
    if(fread(base_palette,1,768,f)!=768){
        fprintf(stderr,"Error: %s is not 768 bytes\n",fn);
        fclose(f); exit(1);
    }
    fclose(f);
    for(int i=0;i<256;i++){
        for(int c=0;c<3;c++){
            uint8_t v=base_palette[i][c];
            disp_palette[i][c] = (v<<2)|(v>>4);
        }
        selected[i]=changed[i]=false;
    }
}

void save_chasm(const char *fn){
    FILE *f=fopen(fn,"wb");
    if(!f){ perror(fn); return; }
    for(int i=0;i<256;i++){
        fputc(disp_palette[i][0]>>2, f);
        fputc(disp_palette[i][1]>>2, f);
        fputc(disp_palette[i][2]>>2, f);
    }
    fclose(f);
    printf("Saved %s\n",fn);
}

//── Color conversions ──────────────────────────────────────────

void rgb2hsl(uint8_t r,uint8_t g,uint8_t b,float &h,float &s,float &l){
    float fr=r/255.0f, fg=g/255.0f, fb=b/255.0f;
    float mx=fmaxf(fmaxf(fr,fg),fb), mn=fminf(fminf(fr,fg),fb);
    l=(mx+mn)*0.5f;
    if(mx==mn){ h=s=0; return; }
    float d=mx-mn;
    s = l>0.5f ? d/(2-mx-mn) : d/(mx+mn);
    if(mx==fr)      h=fmodf((fg-fb)/d + (fg<fb?6:0),6)*60.0f;
    else if(mx==fg) h=((fb-fr)/d +2)*60.0f;
    else            h=((fr-fg)/d +4)*60.0f;
}

void hsl2rgb(float h,float s,float l,uint8_t &r,uint8_t &g,uint8_t &b){
    float c=(1-fabsf(2*l-1))*s;
    float hp=fmodf(h,360.0f)/60.0f;
    float x=c*(1-fabsf(fmodf(hp,2)-1));
    float m=l-c/2, r1,g1,b1;
    if(hp<1){ r1=c; g1=x; b1=0; }
    else if(hp<2){ r1=x; g1=c; b1=0; }
    else if(hp<3){ r1=0; g1=c; b1=x; }
    else if(hp<4){ r1=0; g1=x; b1=c; }
    else if(hp<5){ r1=x; g1=0; b1=c; }
    else         { r1=c; g1=0; b1=x; }
    r=(uint8_t)((r1+m)*255);
    g=(uint8_t)((g1+m)*255);
    b=(uint8_t)((b1+m)*255);
}

//── Helpers ────────────────────────────────────────────────────

void drawText(int x,int y,const char *s){
    glRasterPos2i(x,y);
    while(*s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *s++);
}

void apply_hsl(){
    for(int i=0;i<256;i++){
        if(!selected[i]) continue;
        float h,s,l; rgb2hsl(disp_palette[i][0],disp_palette[i][1],disp_palette[i][2],h,s,l);
        h = fmodf(h + hue_shift + 360.0f,360.0f);
        s = std::clamp(s * saturation, 0.0f,1.0f);
        l = std::clamp(l * lightness,  0.0f,1.0f);
        hsl2rgb(h,s,l,
          disp_palette[i][0],
          disp_palette[i][1],
          disp_palette[i][2]);
        changed[i] = true;
    }
    hue_shift=0; saturation=1; lightness=1;
}

//── GLUT callbacks ─────────────────────────────────────────────

void display(){
    glClear(GL_COLOR_BUFFER_BIT);

    const float gap=1.0f;
    const float cw=(PREV_W - 15*gap)/16.0f;
    const float ch=(PREV_H - 15*gap)/16.0f;

    // draw palette cells
    for(int i=0;i<256;i++){
        int cx=i%16, cy=i/16;
        float px=cx*(cw+gap), py=PREV_H - ((cy+1)*(ch+gap)) + gap;
        glColor3ubv(disp_palette[i]);
        glBegin(GL_QUADS);
          glVertex2f(px,    py);
          glVertex2f(px+cw, py);
          glVertex2f(px+cw, py+ch);
          glVertex2f(px,    py+ch);
        glEnd();
    }

    // yellow outlines for changed
    glColor3ub(255,255,0); glLineWidth(2.0f);
    for(int i=0;i<256;i++) if(changed[i]){
        int cx=i%16, cy=i/16;
        float px=cx*(cw+gap), py=PREV_H - ((cy+1)*(ch+gap)) + gap;
        glBegin(GL_LINE_LOOP);
          glVertex2f(px,    py);
          glVertex2f(px+cw, py);
          glVertex2f(px+cw, py+ch);
          glVertex2f(px,    py+ch);
        glEnd();
    }

    // red outlines for selected
    glColor3ub(255,0,0); glLineWidth(1.0f);
    for(int i=0;i<256;i++) if(selected[i]){
        int cx=i%16, cy=i/16;
        float px=cx*(cw+gap), py=PREV_H - ((cy+1)*(ch+gap)) + gap;
        glBegin(GL_LINE_LOOP);
          glVertex2f(px,    py);
          glVertex2f(px+cw, py);
          glVertex2f(px+cw, py+ch);
          glVertex2f(px,    py+ch);
        glEnd();
    }

    // multiline help text at top
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glColor3f(1,1,1);
    int y0 = PREV_H + TEXT_H - 10;
    char buf[64];
    sprintf(buf,"H:%.0f°  S:%.2f  L:%.2f",hue_shift,saturation,lightness);
    drawText(10, y0,    buf);
    drawText(10, y0-14, "Q/A Hue+/-   W/S Saturation+/-   E/D Lightness+/-");
    drawText(10, y0-28, "R Reload All   T Reset Selected   F5 Select All   F6 Deselect All");
    drawText(10, y0-42, "F12 Save   Esc Quit");

    glutSwapBuffers();
}

void reshape(int w,int h){
    glutReshapeWindow(WIN_W,WIN_H);
    glViewport(0,0,WIN_W,WIN_H);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0,WIN_W,0,WIN_H,-1,1);
    glMatrixMode(GL_MODELVIEW);
}

void keyboard(unsigned char k,int,int){
    switch(k){
      case 'q': hue_shift+=10;  apply_hsl(); break;
      case 'a': hue_shift-=10;  apply_hsl(); break;
      case 'w': saturation+=0.1f; apply_hsl(); break;
      case 's': saturation-=0.1f; apply_hsl(); break;
      case 'e': lightness+=0.1f; apply_hsl(); break;
      case 'd': lightness-=0.1f; apply_hsl(); break;
      case 'r': case 'R': load_chasm("CHASM2.PAL"); break;
      case 't': case 'T':
        for(int i=0;i<256;i++) if(selected[i]){
          uint8_t *bp=base_palette[i], *dp=disp_palette[i];
          dp[0]=(bp[0]<<2)|(bp[0]>>4);
          dp[1]=(bp[1]<<2)|(bp[1]>>4);
          dp[2]=(bp[2]<<2)|(bp[2]>>4);
          changed[i]=false;
        }
        break;
      case 27: exit(0);
    }
    glutPostRedisplay();
}

void special(int key,int,int){
    if(key==GLUT_KEY_F5){
        for(int i=0;i<256;i++) selected[i]=true;
    }
    else if(key==GLUT_KEY_F6){
        for(int i=0;i<256;i++) selected[i]=false;
    }
    else if(key==GLUT_KEY_F12){
        save_chasm("CHASM2_OUT.pal");
        static uint8_t img[256*3];
        for(int i=0;i<256;i++){
            img[3*i+0]=disp_palette[i][0];
            img[3*i+1]=disp_palette[i][1];
            img[3*i+2]=disp_palette[i][2];
        }
        stbi_write_png("palette_out.png",256,1,3,img,256*3);
        printf("Saved palette_out.png\n");
    }
    glutPostRedisplay();
}

static bool dragging=false;
static int  drag0=0;
static bool dragModeSelect=true;

void mouse(int b,int s,int x,int y){
    if(b!=GLUT_LEFT_BUTTON) return;
    int yy=WIN_H - y;
    const float gap=1, cw=(PREV_W-15*gap)/16, ch=(PREV_H-15*gap)/16;
    if(s==GLUT_DOWN && yy>0 && yy<PREV_H){
        int cx=int(x/(cw+gap)), cy=int((PREV_H-yy)/(ch+gap));
        if(cx>=0&&cx<16&&cy>=0&&cy<16){
            drag0=cy*16+cx;
            dragModeSelect = !selected[drag0];
            dragging=true;
        }
    } else dragging=false;
}

void motion(int x,int y){
    if(!dragging) return;
    int yy=WIN_H - y;
    const float gap=1, cw=(PREV_W-15*gap)/16, ch=(PREV_H-15*gap)/16;
    int cx=int(x/(cw+gap)), cy=int((PREV_H-yy)/(ch+gap));
    if(cx<0||cx>15||cy<0||cy>15) return;
    int idx2=cy*16+cx;
    int x1=drag0%16, y1=drag0/16, x2=idx2%16, y2=idx2/16;
    int xa=std::min(x1,x2), xb=std::max(x1,x2);
    int ya=std::min(y1,y2), yb=std::max(y1,y2);
    for(int ry=ya;ry<=yb;ry++){
        for(int rx=xa;rx<=xb;rx++){
            selected[ry*16+rx]=dragModeSelect;
        }
    }
    glutPostRedisplay();
}

int main(int argc,char**argv){
    load_chasm("CHASM2.PAL");
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA);
    glutInitWindowSize(WIN_W,WIN_H);
    glutCreateWindow("Chasm The Rift CHASM2.PAL Editor v1.0.1 by SMR9000");

    glClearColor(0,0,0,1);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);

    glutMainLoop();
    return 0;
}
