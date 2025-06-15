// Paledit100.cpp (v108)
// x86_64-w64-mingw32-g++ paledit108-OK.cpp   -I. -ISTB -Llib -lfreeglut -lopengl32 -lglu32 -lm   -static-libstdc++ -static-libgcc   -o PALEDIT.exe paledit.res

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "STB/stb_image_write.h"

#include <GL/freeglut.h>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <algorithm>

static uint8_t base_palette[256][3];
static bool    selected[256];
static float   hue_shift   = 0.0f,
               saturation  = 1.0f,
               lightness   = 1.0f;

static const int PREV_W = 512;
static const int PREV_H = 512;
static const int TEXT_H =  64;
static const int WIN_W  = PREV_W;
static const int WIN_H  = PREV_H + TEXT_H;

static bool  dragging   = false;
static int   drag_start = 0;
static bool  drag_mode  = true;  // true = select, false = deselect

void rgb_to_hsl(uint8_t r,uint8_t g,uint8_t b,float &h,float &s,float &l){
    float fr=r/255.0f, fg=g/255.0f, fb=b/255.0f;
    float mx=fmaxf(fmaxf(fr,fg),fb), mn=fminf(fminf(fr,fg),fb);
    l=(mx+mn)*0.5f;
    if(mx==mn){h=s=0;return;}
    float d=mx-mn;
    s = l>0.5f ? d/(2-mx-mn) : d/(mx+mn);
    if(mx==fr)      h=fmodf((fg-fb)/d + (fg<fb?6:0),6.0f)*60.0f;
    else if(mx==fg) h=((fb-fr)/d +2)*60.0f;
    else            h=((fr-fg)/d +4)*60.0f;
}

void hsl_to_rgb(float h,float s,float l,uint8_t &r,uint8_t &g,uint8_t &b){
    float c=(1-fabsf(2*l-1))*s;
    float hp=fmodf(h,360.0f)/60.0f, x=c*(1-fabsf(fmodf(hp,2)-1)), m=l-c/2;
    float r1,g1,b1;
    if(hp<1){r1=c;g1=x;b1=0;}
    else if(hp<2){r1=x;g1=c;b1=0;}
    else if(hp<3){r1=0;g1=c;b1=x;}
    else if(hp<4){r1=0;g1=x;b1=c;}
    else if(hp<5){r1=x;g1=0;b1=c;}
    else{r1=c;g1=0;b1=x;}
    r=(uint8_t)((r1+m)*255);
    g=(uint8_t)((g1+m)*255);
    b=(uint8_t)((b1+m)*255);
}

void get_adjusted(int i,uint8_t &r,uint8_t &g,uint8_t &b){
    if(!selected[i]){
        r=base_palette[i][0];
        g=base_palette[i][1];
        b=base_palette[i][2];
        return;
    }
    float h,s,l;
    rgb_to_hsl(base_palette[i][0],base_palette[i][1],base_palette[i][2],h,s,l);
    h=fmodf(h+hue_shift+360,360);
    s=std::clamp(s*saturation,0.0f,1.0f);
    l=std::clamp(l*lightness, 0.0f,1.0f);
    hsl_to_rgb(h,s,l,r,g,b);
}

void load_chasm_palette(const char *fn){
    FILE *f=fopen(fn,"rb");
    if(!f){fprintf(stderr,"Error: cannot open %s\n",fn);exit(1);}
    if(fread(base_palette,1,768,f)!=768){
        fprintf(stderr,"Error: %s is not 768 bytes\n",fn);
        fclose(f);exit(1);
    }
    fclose(f);
    for(int i=0;i<256;i++)for(int c=0;c<3;c++)
        base_palette[i][c]=(base_palette[i][c]<<2)|(base_palette[i][c]>>4);
    for(int i=0;i<256;i++)selected[i]=true;
}

void save_chasm_palette(const char *fn){
    FILE *f=fopen(fn,"wb");
    if(!f){fprintf(stderr,"Error: cannot write %s\n",fn);return;}
    for(int i=0;i<256;i++){
        uint8_t r,g,b; get_adjusted(i,r,g,b);
        fputc(r>>2,f); fputc(g>>2,f); fputc(b>>2,f);
    }
    fclose(f);
    printf("Saved %s\n",fn);
}

void drawText(int x,int y,const char *s){
    glRasterPos2i(x,y);
    while(*s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10,*s++);
}

void display(){
    glClear(GL_COLOR_BUFFER_BIT);

    // draw grid
    const float sp=1.0f;
    const float cell_w=(PREV_W-15*sp)/16.0f;
    const float cell_h=(PREV_H-15*sp)/16.0f;
    for(int i=0;i<256;i++){
        int cx=i%16, cy=i/16;
        float px=cx*(cell_w+sp);
        float py=TEXT_H + PREV_H - ((cy+1)*(cell_h+sp)) + sp;
        uint8_t r,g,b; get_adjusted(i,r,g,b);
        glColor3ub(r,g,b);
        glBegin(GL_QUADS);
            glVertex2f(px,      py);
            glVertex2f(px+cell_w,py);
            glVertex2f(px+cell_w,py+cell_h);
            glVertex2f(px,      py+cell_h);
        glEnd();
    }

    // outlines
    glLineWidth(1.0f);
    glColor3ub(255,0,0);
    for(int i=0;i<256;i++) if(selected[i]){
        int cx=i%16, cy=i/16;
        float px=cx*(cell_w+sp);
        float py=TEXT_H + PREV_H - ((cy+1)*(cell_h+sp)) + sp;
        glBegin(GL_LINE_LOOP);
            glVertex2f(px,      py);
            glVertex2f(px+cell_w,py);
            glVertex2f(px+cell_w,py+cell_h);
            glVertex2f(px,      py+cell_h);
        glEnd();
    }

    // text below
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glColor3f(1,1,1);
    //drawText(10,20,"Chasm The Rift CHASM2.PAL Editor v1.0.0");
    char buf[64];
    sprintf(buf,"H:%.0f°  S:%.2f  L:%.2f",hue_shift,saturation,lightness);
    drawText(10,30,buf);
    drawText(10,40,
      "Q/A: Hue+/-   W/S: Saturation+/-   E/D: Lightness+/-   R: Reset   F5: All   F6: None   F12: Save");

    glutSwapBuffers();
}

void reshape(int w,int h){
    glutReshapeWindow(WIN_W,WIN_H);
}

void keyboard(unsigned char key,int x,int y){
    switch(key){
      case 'q': hue_shift+=10; break;
      case 'a': hue_shift-=10; break;
      case 'w': saturation+=0.1f; break;
      case 's': saturation-=0.1f; break;
      case 'e': lightness+=0.1f; break;
      case 'd': lightness-=0.1f; break;
      case 'r': case 'R':
        hue_shift=0; saturation=1; lightness=1; printf("Reset\n"); break;
      case 27: exit(0);
    }
    hue_shift=fmodf(hue_shift+360,360);
    saturation=std::clamp(saturation,0.0f,2.0f);
    lightness =std::clamp(lightness, 0.0f,2.0f);
    glutPostRedisplay();
}

void special(int key,int x,int y){
    if(key==GLUT_KEY_F5){
        for(int i=0;i<256;i++)selected[i]=true;
        glutPostRedisplay();
    } else if(key==GLUT_KEY_F6){
        for(int i=0;i<256;i++)selected[i]=false;
        glutPostRedisplay();
    } else if(key==GLUT_KEY_F12){
        save_chasm_palette("CHASM2_OUT.pal");
        static uint8_t img[256*3];
        for(int i=0;i<256;i++){
            uint8_t r,g,b; get_adjusted(i,r,g,b);
            img[3*i+0]=r; img[3*i+1]=g; img[3*i+2]=b;
        }
        stbi_write_png("palette_out.png",256,1,3,img,256*3);
        printf("Also wrote palette_out.png\n");
    }
}

void mouse(int button,int state,int x,int y){
    if(button==GLUT_LEFT_BUTTON){
        if(state==GLUT_DOWN){
            int yy=WIN_H-y;
            const float sp=1.0f, cw=(PREV_W-15*sp)/16.0f, ch=(PREV_H-15*sp)/16.0f;
            if(yy>=TEXT_H && yy<=TEXT_H+PREV_H){
                int cx=int(x/(cw+sp)), cy=int((PREV_H-(yy-TEXT_H))/(ch+sp));
                if(cx>=0&&cx<16&&cy>=0&&cy<16){
                    int idx=cy*16+cx;
                    drag_start=idx;
                    drag_mode=!selected[idx];
                    dragging=true;
                }
            }
        } else {
            dragging=false;
        }
    }
}

void motion(int x,int y){
    if(!dragging) return;
    int yy=WIN_H-y;
    const float sp=1.0f, cw=(PREV_W-15*sp)/16.0f, ch=(PREV_H-15*sp)/16.0f;
    int cx=int(x/(cw+sp)), cy=int((PREV_H-(yy-TEXT_H))/(ch+sp));
    if(cx<0||cx>=16||cy<0||cy>=16) return;
    int idx2=cy*16+cx;
    int x1=drag_start%16, y1=drag_start/16;
    int x2=idx2%16,    y2=idx2/16;
    int xa=std::min(x1,x2), xb=std::max(x1,x2);
    int ya=std::min(y1,y2), yb=std::max(y1,y2);
    for(int ry=ya;ry<=yb;ry++)
        for(int rx=xa;rx<=xb;rx++)
            selected[ry*16+rx]=drag_mode;
    glutPostRedisplay();
}

int main(int argc,char**argv){
    if(argc!=2){fprintf(stderr,"Usage: %s CHASM2.PAL\n",argv[0]);return 1;}
    load_chasm_palette(argv[1]);

    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA);
    glutInitWindowSize(WIN_W,WIN_H);
    glutCreateWindow("Chasm The Rift CHASM2.PAL Editor v1.0.0 by SMR9000");

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0,WIN_W,0,WIN_H,-1,1);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);

    glutMainLoop();
    return 0;
}
