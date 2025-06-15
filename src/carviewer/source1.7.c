// ld -r -b binary -o chasmpalette.o chasmpalette.act
// x86_64-w64-mingw32-gcc source1.7.c -o carviewer.exe -Iinclude -Llib -lfreeglut -lopengl32 -lglu32 carviewer.res chasmpalette.o
// this version no longer neeeds external chasmpalette.act

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <GL/gl.h>
#include <GL/freeglut.h>

// Constants
#define SCALE      (1.0f / 2048.0f)
#define TEX_WIDTH  64

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
    uint8_t  group_id;
    uint8_t  flags;
    uint16_t v_offset;
} CARPolygon;

typedef struct {
    int16_t xyz[3];
} Vertex;
#pragma pack(pop)

// Extern symbols for embedded palette
extern unsigned char _binary_chasmpalette_act_start[];
extern unsigned char _binary_chasmpalette_act_end[];

// Globals
static uint8_t    *rawData      = NULL;
static size_t      rawSize      = 0;
static uint8_t     paletteRGB[256][3];
static uint8_t    *textureRGBA  = NULL;
static uint16_t    texWidth, texHeight;
static Vertex     *vertices     = NULL;
static CARPolygon *polygons     = NULL;
static size_t      vertexCount  = 0, polygonCount = 0;
static GLuint      texID;

// Background color & palette cycling
static float bgColor[3] = {0.2f, 0.2f, 0.3f};
static int initBgPaletteIndex    = 0;
static int currentBgPaletteIndex = 0;

// Model center for rotation
static float modelCenterX = 0.0f;
static float modelCenterY = 0.0f;
static float modelCenterZ = 0.0f;

// View state
static float initRotateX    = -90.0f;
static float initRotateY    =   0.0f;
static float initTranslateX =   0.0f;
static float initTranslateY =   0.0f;
static float initZoom       =   2.5f;
static float rotateX    = -90.0f;
static float rotateY    =  0.0f;
static float translateX =  0.0f;
static float translateY =  0.0f;
static float zoom       =  2.5f;
static int   lastMouseX, lastMouseY, leftButtonDown;
static int   wireframeMode   = 0;
static int   linearFiltering = 0;
static int   spinning        = 1;
static int   overlayEnabled  = 1;

// Window size
static int winWidth = 800, winHeight = 600;

// Load palette from embedded binary
void load_palette_embedded(void) {
    unsigned char *p = _binary_chasmpalette_act_start;
    size_t len = _binary_chasmpalette_act_end - _binary_chasmpalette_act_start;
    if (len != 256*3) {
        fprintf(stderr, "Embedded palette wrong size (%zu bytes)\n", len);
        exit(1);
    }
    for (int i = 0; i < 256; ++i) {
        paletteRGB[i][0] = p[3*i + 0];
        paletteRGB[i][1] = p[3*i + 1];
        paletteRGB[i][2] = p[3*i + 2];
    }
}

// Stop autorotate
static void stopSpin() { spinning = 0; }

// Idle callback for spinning
void idle(void) {
    if (spinning) {
        rotateY += 0.2f;
        glutPostRedisplay();
    }
}

// Load and decode the .car model
void load_car_model(const char *fn) {
    FILE *f = fopen(fn, "rb");
    if (!f) { perror("Model file"); exit(1); }
    fseek(f, 0, SEEK_END);
    rawSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    rawData = malloc(rawSize);
    if (!rawData) { perror("Alloc rawData"); exit(1); }
    fread(rawData, 1, rawSize, f);
    fclose(f);

    vertexCount  = *(uint16_t*)(rawData + 0x4866);
    polygonCount = *(uint16_t*)(rawData + 0x4868);
    uint16_t texels = *(uint16_t*)(rawData + 0x486A);
    texWidth  = TEX_WIDTH;
    texHeight = texels / TEX_WIDTH;

    // Decode indexed texture to RGBA
    size_t texOffset = 0x486C;
    uint8_t *indices = rawData + texOffset;
    textureRGBA = malloc(texWidth * texHeight * 4);
    if (!textureRGBA) { perror("Alloc textureRGBA"); exit(1); }
    for (size_t i = 0; i < texWidth * texHeight; ++i) {
        uint8_t idx = indices[i];
        textureRGBA[4*i + 0] = paletteRGB[idx][0];
        textureRGBA[4*i + 1] = paletteRGB[idx][1];
        textureRGBA[4*i + 2] = paletteRGB[idx][2];
        textureRGBA[4*i + 3] = (paletteRGB[idx][0]==4 &&
                                paletteRGB[idx][1]==4 &&
                                paletteRGB[idx][2]==4) ? 0 : 255;
    }

    // Pick a bright texel as background color
    {
        int counts[256] = {0};
        for (size_t i = 0; i < texWidth * texHeight; ++i) {
            uint8_t idx = indices[i];
            float bright = (paletteRGB[idx][0] +
                            paletteRGB[idx][1] +
                            paletteRGB[idx][2]) / (3.0f * 255.0f);
            if (bright <= 0.2f) continue;
            counts[idx]++;
        }
        int best = -1, bc = 0;
        for (int c = 0; c < 256; ++c) {
            if (counts[c] > bc) { bc = counts[c]; best = c; }
        }
        if (best >= 0) {
            initBgPaletteIndex    = best;
            currentBgPaletteIndex = best;
            bgColor[0] = paletteRGB[best][0] / 255.0f;
            bgColor[1] = paletteRGB[best][1] / 255.0f;
            bgColor[2] = paletteRGB[best][2] / 255.0f;
        }
    }

    // Upload to OpenGL
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texWidth, texHeight, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, textureRGBA);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Geometry pointers
    vertices = (Vertex*)(rawData + texOffset + texels);
    polygons = (CARPolygon*)(rawData + 0x66);

    // Compute model center for rotation
    {
        float minX=1e9f, minY=1e9f, minZ=1e9f;
        float maxX=-1e9f, maxY=-1e9f, maxZ=-1e9f;
        for (size_t i = 0; i < vertexCount; ++i) {
            float x = vertices[i].xyz[0] * SCALE;
            float y = vertices[i].xyz[1] * SCALE;
            float z = vertices[i].xyz[2] * SCALE;
            if (x < minX) minX = x; if (x > maxX) maxX = x;
            if (y < minY) minY = y; if (y > maxY) maxY = y;
            if (z < minZ) minZ = z; if (z > maxZ) maxZ = z;
        }
        modelCenterX = 0.5f*(minX + maxX);
        modelCenterY = 0.5f*(minY + maxY);
        modelCenterZ = 0.5f*(minZ + maxZ);
    }
}

// Mouse & motion
void mouse(int button,int state,int x,int y) {
    stopSpin();
    if (button == GLUT_LEFT_BUTTON) {
        leftButtonDown = (state == GLUT_DOWN);
        lastMouseX = x; lastMouseY = y;
    }
}
void motion(int x,int y) {
    stopSpin();
    if (leftButtonDown) {
        rotateY += (x - lastMouseX) * 0.5f;
        rotateX += (y - lastMouseY) * 0.5f;
        lastMouseX = x; lastMouseY = y;
        glutPostRedisplay();
    }
}

// Special keys
void special(int key,int x,int y) {
    stopSpin();
    const float pan = 0.1f;
    switch(key) {
        case GLUT_KEY_PAGE_UP:
            currentBgPaletteIndex = (currentBgPaletteIndex + 1) % 256; break;
        case GLUT_KEY_PAGE_DOWN:
            currentBgPaletteIndex = (currentBgPaletteIndex + 255) % 256; break;
        case GLUT_KEY_F1:
            overlayEnabled = !overlayEnabled; break;
        case GLUT_KEY_LEFT:
            translateX -= pan; break;
        case GLUT_KEY_RIGHT:
            translateX += pan; break;
        case GLUT_KEY_UP:
            translateY += pan; break;
        case GLUT_KEY_DOWN:
            translateY -= pan; break;
    }
    bgColor[0] = paletteRGB[currentBgPaletteIndex][0] / 255.0f;
    bgColor[1] = paletteRGB[currentBgPaletteIndex][1] / 255.0f;
    bgColor[2] = paletteRGB[currentBgPaletteIndex][2] / 255.0f;
    glutPostRedisplay();
}

// Keyboard input
void keyboard(unsigned char key,int x,int y) {
    stopSpin();
    switch(key) {
        case 27: // ESC
            rotateX = initRotateX; rotateY = initRotateY;
            translateX = initTranslateX; translateY = initTranslateY;
            zoom = initZoom; spinning = 1;
            currentBgPaletteIndex = initBgPaletteIndex;
            bgColor[0] = paletteRGB[initBgPaletteIndex][0]/255.0f;
            bgColor[1] = paletteRGB[initBgPaletteIndex][1]/255.0f;
            bgColor[2] = paletteRGB[initBgPaletteIndex][2]/255.0f;
            break;
        case 'w': zoom *= 1.1f; break;
        case 's': zoom /= 1.1f; break;
        case 'a': rotateY -= 10.0f; break;
        case 'd': rotateY += 10.0f; break;
        case 'r': spinning = !spinning; break;
        case '\t':
            wireframeMode = !wireframeMode;
            glPolygonMode(GL_FRONT_AND_BACK,
                          wireframeMode?GL_LINE:GL_FILL);
            break;
        case 'f':
            linearFiltering = !linearFiltering;
            glBindTexture(GL_TEXTURE_2D, texID);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                            linearFiltering?GL_LINEAR:GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                            linearFiltering?GL_LINEAR:GL_NEAREST);
            break;
    }
    glutPostRedisplay();
}

// Overlay text
void drawBitmapString(float x,float y,void* font,const char* s) {
    glRasterPos2f(x, y);
    for (; *s; ++s) glutBitmapCharacter(font, *s);
}
void drawOverlay() {
    glMatrixMode(GL_PROJECTION);
      glPushMatrix(); glLoadIdentity();
      gluOrtho2D(0, winWidth, 0, winHeight);
    glMatrixMode(GL_MODELVIEW);
      glPushMatrix(); glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glColor3f(0,0,0);
    const char* lines[] = {
        "", "", "F1: Toggle overlay", "ESC: Reset view",
        "W/S: Zoom", "A/D: Rotate", "R: Spin ON/OFF",
        "TAB: Wireframe", "F: Texture filter",
        "Arrows: Pan", "PgUp/Dn: Cycle BG", "Mouse drag: Rotate"
    };
    for (int i = 0; i < 12; ++i)
        drawBitmapString(10, winHeight-12*(i+1),
                         GLUT_BITMAP_HELVETICA_12, lines[i]);
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
      glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// Display callback
void display(void) {
    glClearColor(bgColor[0], bgColor[1], bgColor[2], 1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glColor3f(1,1,1);
    glLoadIdentity();
    glTranslatef(translateX, translateY, -5.0f/zoom);
    glTranslatef(modelCenterX, modelCenterY, modelCenterZ);
    glRotatef(rotateY,0,1,0); glRotatef(rotateX,1,0,0);
    glTranslatef(-modelCenterX, -modelCenterY, -modelCenterZ);

    glBindTexture(GL_TEXTURE_2D, texID);
    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < polygonCount; ++i) {
        CARPolygon *p = &polygons[i];
        int isQuad = p->vertices_indices[3] < (int)vertexCount;
        // first triangle
        for (int v=0; v<3; ++v) {
            int vi = p->vertices_indices[v];
            glTexCoord2f(
                p->uv[v][0]/(float)(texWidth<<8),
                (p->uv[v][1]+4*p->v_offset)/(float)(texHeight<<8));
            glVertex3f(
                vertices[vi].xyz[0]*SCALE,
                vertices[vi].xyz[1]*SCALE,
                vertices[vi].xyz[2]*SCALE);
        }
        // second triangle if quad
        if (isQuad) {
            int ord[3] = {0,2,3};
            for (int t=0; t<3; ++t) {
                int vi = p->vertices_indices[ord[t]];
                glTexCoord2f(
                    p->uv[ord[t]][0]/(float)(texWidth<<8),
                    (p->uv[ord[t]][1]+4*p->v_offset)/(float)(texHeight<<8));
                glVertex3f(
                    vertices[vi].xyz[0]*SCALE,
                    vertices[vi].xyz[1]*SCALE,
                    vertices[vi].xyz[2]*SCALE);
            }
        }
    }
    glEnd();
    if (overlayEnabled) drawOverlay();
    glutSwapBuffers();
}

// Window resize
void reshape(int w,int h) {
    winWidth = w; winHeight = h;
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      gluPerspective(45.0f, (float)w/h, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
}

// Main
int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr,"Usage: %s <model.car>\n", argv[0]);
        return 1;
    }
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA|GLUT_DEPTH);
    glutInitWindowSize(winWidth, winHeight);
    glutCreateWindow("Chasm The Rift CAR Viewer (embedded palette)");

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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
