// ld -r -b binary -o chasmpalette.o chasmpalette.act
// x86_64-w64-mingw32-gcc source1.9.c -o carviewer.exe -Iinclude -Llib -lfreeglut -lopengl32 -lglu32 carviewer.res chasmpalette.o
// this version no longer neeeds external chasmpalette.act

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <GL/gl.h>
#include <GL/freeglut.h>

// ---- Constants ----
#define SCALE      (1.0f / 2048.0f)   // Scale factor for fixed-point vertex data
#define TEX_WIDTH  64                 // Texture width in pixels

#pragma pack(push,1)

// ---- File header structures ----
typedef struct {
    uint16_t animations[20];          // Offsets for each animation block
    uint16_t submodels_animations[3][2]; 
    uint16_t unknown0[9];
    uint16_t sounds[7];
    uint16_t unknown1[9];
} CARHeader;

typedef struct {
    uint16_t vertices_indices[4];     // Indices of vertices for this polygon
    uint16_t uv[4][2];                // Texture coordinates
    uint8_t  unknown0[4];
    uint8_t  group_id;
    uint8_t  flags;
    uint16_t v_offset;                // Vertical texture offset
} CARPolygon;

typedef struct {
    int16_t xyz[3];                   // Vertex coordinates (fixed-point)
} Vertex;

#pragma pack(pop)

// ---- External palette data (linked binary) ----
extern unsigned char _binary_chasmpalette_act_start[];
extern unsigned char _binary_chasmpalette_act_end[];

// ---- Global variables ----
static uint8_t    *rawData      = NULL;    // Raw file data
static size_t      rawSize      = 0;
static uint8_t     paletteRGB[256][3];     // Loaded palette (RGB)
static uint8_t    *textureRGBA  = NULL;    // Decoded texture (RGBA)
static uint16_t    texWidth, texHeight;
static Vertex     *animationFrames = NULL; // All vertex frames
static size_t      vertexCount  = 0;
static size_t      polygonCount = 0;
static size_t      frameCount   = 0;
static float       animationTime = 0.0f;   // Interpolation accumulator
static float       frameDuration = 0.1f;   // Seconds per frame
static int         animating    = 0;       // Animation play/pause flag

typedef struct { size_t start, count; } AnimInfo;
static AnimInfo   anims[20];               // Animation info
static int        animCount     = 0;       // Number of animations
static int        currentAnim   = 0;       // Index of current animation
static size_t     animFrameIdx  = 0;       // Frame index within current animation

static CARPolygon *polygons     = NULL;    // Polygon data
static GLuint      texID;                  // OpenGL texture ID

// Background color and palette cycling
static float bgColor[3] = {0.2f,0.2f,0.3f};
static int initBgPaletteIndex=0;
static int currentBgPaletteIndex=0;

// Model centering
static float modelCenterX, modelCenterY, modelCenterZ;

// View controls
static float initRotateX=-90, initRotateY=0;
static float initTranslateX=0, initTranslateY=0;
static float initZoom=2.5f;
static float rotateX=-90, rotateY=0, translateX=0, translateY=0, zoom=2.5f;
static int lastMouseX, lastMouseY, leftButtonDown=0;
static int wireframeMode=0, linearFiltering=0;
static int spinning=1, overlayEnabled=1;

// Window size
static int winWidth=800, winHeight=600;

// ---- Utility functions ----

// Load embedded palette into paletteRGB
void load_palette_embedded(void) {
    unsigned char *p = _binary_chasmpalette_act_start;
    size_t len = _binary_chasmpalette_act_end - p;
    if (len != 256*3) {
        fprintf(stderr, "Wrong palette size: %zu\n", len);
        exit(1);
    }
    for (int i = 0; i < 256; i++) {
        paletteRGB[i][0] = p[3*i + 0];
        paletteRGB[i][1] = p[3*i + 1];
        paletteRGB[i][2] = p[3*i + 2];
    }
}

// Stop automatic spinning
static void stopSpin() { spinning = 0; }

// ---- Model and texture loading ----

void load_car_model(const char *fn) {
    // Open and read the entire .car file
    FILE *f = fopen(fn, "rb");
    if (!f) { perror("Model"); exit(1); }
    fseek(f, 0, SEEK_END);
    rawSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    rawData = malloc(rawSize);
    fread(rawData, 1, rawSize, f);
    fclose(f);

    // Read vertex & polygon counts and texture size
    vertexCount = *(uint16_t*)(rawData + 0x4866);
    polygonCount = *(uint16_t*)(rawData + 0x4868);
    uint16_t texels = *(uint16_t*)(rawData + 0x486A);
    texWidth = TEX_WIDTH;
    texHeight = texels / TEX_WIDTH;

    // Decode indexed texture to RGBA
    size_t texOffset = 0x486C;
    uint8_t *indices = rawData + texOffset;
    textureRGBA = malloc(texWidth * texHeight * 4);
    for (size_t i = 0; i < texWidth * texHeight; i++) {
        uint8_t idx = indices[i];
        textureRGBA[4*i + 0] = paletteRGB[idx][0];
        textureRGBA[4*i + 1] = paletteRGB[idx][1];
        textureRGBA[4*i + 2] = paletteRGB[idx][2];
        // Transparent if pure index color of (4,4,4)
        textureRGBA[4*i + 3] = (paletteRGB[idx][0]==4 && paletteRGB[idx][1]==4 && paletteRGB[idx][2]==4) ? 0 : 255;
    }
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureRGBA);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Calculate total animation frames
    uint8_t *fd = rawData + texOffset + texels;
    size_t bytesLeft = rawSize - (fd - rawData);
    frameCount = bytesLeft / (vertexCount * sizeof(Vertex));
    animationFrames = (Vertex*)fd;

    // Build animation info from header
    CARHeader *hdr = (CARHeader*)rawData;
    size_t off = 0;
    animCount = 0;
    for (int i = 0; i < 20; i++) {
        uint16_t b = hdr->animations[i];
        if (b > 0) {
            size_t nframes = b / (vertexCount * sizeof(Vertex));
            anims[animCount].start = off;
            anims[animCount].count = nframes;
            off += nframes;
            animCount++;
        }
    }
    // If none specified, treat whole file as single animation
    if (animCount == 0) {
        anims[0].start = 0;
        anims[0].count = frameCount;
        animCount = 1;
    }
    currentAnim = 0;
    animFrameIdx = 0;

    // Load polygons array
    polygons = (CARPolygon*)(rawData + 0x66);

    // Determine initial background color from most frequent bright index
    int counts[256] = {0};
    for (size_t i = 0; i < texWidth * texHeight; i++) {
        uint8_t idx = indices[i];
        float bright = (paletteRGB[idx][0] + paletteRGB[idx][1] + paletteRGB[idx][2]) / (3.0f * 255.0f);
        if (bright > 0.2f) counts[idx]++;
    }
    int best = 0, bc = 0;
    for (int c = 0; c < 256; c++) {
        if (counts[c] > bc) { bc = counts[c]; best = c; }
    }
    initBgPaletteIndex = currentBgPaletteIndex = best;
    bgColor[0] = paletteRGB[best][0]/255.0f;
    bgColor[1] = paletteRGB[best][1]/255.0f;
    bgColor[2] = paletteRGB[best][2]/255.0f;

    // Compute model center for rotation
    float minX = 1e9f, minY = 1e9f, minZ = 1e9f;
    float maxX = -1e9f, maxY = -1e9f, maxZ = -1e9f;
    for (size_t i = 0; i < vertexCount; i++) {
        float x = animationFrames[i].xyz[0] * SCALE;
        float y = animationFrames[i].xyz[1] * SCALE;
        float z = animationFrames[i].xyz[2] * SCALE;
        if (x < minX) minX = x; if (x > maxX) maxX = x;
        if (y < minY) minY = y; if (y > maxY) maxY = y;
        if (z < minZ) minZ = z; if (z > maxZ) maxZ = z;
    }
    modelCenterX = (minX + maxX) * 0.5f;
    modelCenterY = (minY + maxY) * 0.5f;
    modelCenterZ = (minZ + maxZ) * 0.5f;
}

// ---- Callbacks ----

// Idle: update animation and optionally spin
void idle(void) {
    static int lastTime = 0;
    int time = glutGet(GLUT_ELAPSED_TIME);
    float dt = (time - lastTime) / 1000.0f;
    lastTime = time;
    if (spinning) rotateY += 0.2f;
    if (animating && anims[currentAnim].count > 1) {
        animationTime += dt;
        if (animationTime >= frameDuration) {
            animationTime -= frameDuration;
            animFrameIdx = (animFrameIdx + 1) % anims[currentAnim].count;
        }
    }
    glutPostRedisplay();
}

// Mouse press: stop spinning, track drag start
void mouse(int button, int state, int x, int y) {
    stopSpin();
    if (button == GLUT_LEFT_BUTTON) {
        leftButtonDown = (state == GLUT_DOWN);
        lastMouseX = x;
        lastMouseY = y;
    }
}

// Mouse drag: rotate model
void motion(int x, int y) {
    stopSpin();
    if (leftButtonDown) {
        rotateY += (x - lastMouseX) * 0.5f;
        rotateX += (y - lastMouseY) * 0.5f;
        lastMouseX = x;
        lastMouseY = y;
        glutPostRedisplay();
    }
}

// Special keys: page up/down for BG cycle, arrow keys pan, F1 toggle overlay
void special(int key, int x, int y) {
    stopSpin();
    float pan = 0.1f;
    switch (key) {
        case GLUT_KEY_PAGE_UP:
            currentBgPaletteIndex = (currentBgPaletteIndex + 1) % 256;
            break;
        case GLUT_KEY_PAGE_DOWN:
            currentBgPaletteIndex = (currentBgPaletteIndex + 255) % 256;
            break;
        case GLUT_KEY_F1:
            overlayEnabled = !overlayEnabled;
            break;
        case GLUT_KEY_LEFT:
            translateX -= pan;
            break;
        case GLUT_KEY_RIGHT:
            translateX += pan;
            break;
        case GLUT_KEY_UP:
            translateY += pan;
            break;
        case GLUT_KEY_DOWN:
            translateY -= pan;
            break;
    }
    // Update BG color from palette
    bgColor[0] = paletteRGB[currentBgPaletteIndex][0] / 255.0f;
    bgColor[1] = paletteRGB[currentBgPaletteIndex][1] / 255.0f;
    bgColor[2] = paletteRGB[currentBgPaletteIndex][2] / 255.0f;
    glutPostRedisplay();
}

// Keyboard: all key controls including animation select, +/-, pause, reset
void keyboard(unsigned char key, int x, int y) {
    stopSpin();
    switch (key) {
        case 27:  // ESC: reset view & BG
            rotateX = initRotateX;
            rotateY = initRotateY;
            translateX = initTranslateX;
            translateY = initTranslateY;
            zoom = initZoom;
            spinning = 1;
            currentBgPaletteIndex = initBgPaletteIndex;
            bgColor[0] = paletteRGB[initBgPaletteIndex][0]/255.0f;
            bgColor[1] = paletteRGB[initBgPaletteIndex][1]/255.0f;
            bgColor[2] = paletteRGB[initBgPaletteIndex][2]/255.0f;
            break;
        case 'w': zoom *= 1.1f; break;  // Zoom in
        case 's': zoom /= 1.1f; break;  // Zoom out
        case 'a': rotateY -= 10.0f; break;  // Rotate left
        case 'd': rotateY += 10.0f; break;  // Rotate right
        case 'r': spinning = !spinning; break;  // Toggle spin
        case ' ': animating = !animating; break;  // Pause/play animation
        // Direct animation select 1-0
        case '1': if(animCount>=1) { currentAnim=0; animFrameIdx=0; animationTime=0; } break;
        case '2': if(animCount>=2) { currentAnim=1; animFrameIdx=0; animationTime=0; } break;
        case '3': if(animCount>=3) { currentAnim=2; animFrameIdx=0; animationTime=0; } break;
        case '4': if(animCount>=4) { currentAnim=3; animFrameIdx=0; animationTime=0; } break;
        case '5': if(animCount>=5) { currentAnim=4; animFrameIdx=0; animationTime=0; } break;
        case '6': if(animCount>=6) { currentAnim=5; animFrameIdx=0; animationTime=0; } break;
        case '7': if(animCount>=7) { currentAnim=6; animFrameIdx=0; animationTime=0; } break;
        case '8': if(animCount>=8) { currentAnim=7; animFrameIdx=0; animationTime=0; } break;
        case '9': if(animCount>=9) { currentAnim=8; animFrameIdx=0; animationTime=0; } break;
        case '0': if(animCount>=10){ currentAnim=9; animFrameIdx=0; animationTime=0; } break;
        // Cycle animations with + / - keys
        case '+': case '=':
            if(animCount>0) {
                currentAnim = (currentAnim + 1) % animCount;
                animFrameIdx = 0;
                animationTime = 0;
            }
            break;
        case '-': case '_':
            if(animCount>0) {
                currentAnim = (currentAnim + animCount - 1) % animCount;
                animFrameIdx = 0;
                animationTime = 0;
            }
            break;
        case '\t':  // TAB: toggle wireframe
            wireframeMode = !wireframeMode;
            glPolygonMode(GL_FRONT_AND_BACK, wireframeMode ? GL_LINE : GL_FILL);
            break;
        case 'f':  // F: toggle texture filtering
            linearFiltering = !linearFiltering;
            glBindTexture(GL_TEXTURE_2D, texID);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linearFiltering?GL_LINEAR:GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linearFiltering?GL_LINEAR:GL_NEAREST);
            break;
    }
    glutPostRedisplay();
}

// Draw overlay text using bitmap font
void drawBitmapString(float x, float y, void* font, const char* s) {
    glRasterPos2f(x, y);
    while (*s) {
        glutBitmapCharacter(font, *s++);
    }
}

// Render overlay instructions
void drawOverlay() {
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); gluOrtho2D(0, winWidth, 0, winHeight);
    glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glColor3f(0,0,0);
    const char* lines[] = {
        "F1: Hide this text", 
        "Space: Play/Pause anim", 
        "0-9: Select anim", 
        "+/-: Cycle anim", 
        "Space: Pause/Play", 
        "R: Spin ON/OFF", 
        "ESC: Reset view/bg",
        "W/S: Zoom", 
        "A/D: Rotate", 
        "TAB: Wireframe", 
        "F: Filter", 
        "Arrows: Pan", 
        "PgUp/Dn: Cycle BG", 
        "Mouse drag: Rotate"
    };
    for (int i=0; i<12; i++) {
        drawBitmapString(10, winHeight - 12*(i+1), GLUT_BITMAP_HELVETICA_12, lines[i]);
    }
    glEnable(GL_DEPTH_TEST);
    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
}

// Main render function with interpolation
void display(void) {
    glClearColor(bgColor[0], bgColor[1], bgColor[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glColor3f(1,1,1);
    glLoadIdentity();
    glTranslatef(translateX, translateY, -5.0f/zoom);
    glTranslatef(modelCenterX, modelCenterY, modelCenterZ);
    glRotatef(rotateY, 0,1,0);
    glRotatef(rotateX, 1,0,0);
    glTranslatef(-modelCenterX, -modelCenterY, -modelCenterZ);

    // Compute interpolation factor
    float alpha = (animating && anims[currentAnim].count>1)
        ? (animationTime/frameDuration)
        : 0.0f;
    size_t f0 = anims[currentAnim].start + animFrameIdx;
    size_t f1 = anims[currentAnim].start + ((animFrameIdx+1) % anims[currentAnim].count);

    glBindTexture(GL_TEXTURE_2D, texID);
    glBegin(GL_TRIANGLES);
    for (size_t i=0; i<polygonCount; i++) {
        CARPolygon *p = &polygons[i];
        // Draw first triangle (always exists)
        for (int v=0; v<3; v++) {
            int vi = p->vertices_indices[v];
            int16_t *p0 = animationFrames[f0*vertexCount + vi].xyz;
            int16_t *p2 = animationFrames[f1*vertexCount + vi].xyz;
            float x = (1-alpha)*p0[0] + alpha*p2[0];
            float y = (1-alpha)*p0[1] + alpha*p2[1];
            float z = (1-alpha)*p0[2] + alpha*p2[2];
            glTexCoord2f(
                p->uv[v][0] / (float)(texWidth<<8),
                (p->uv[v][1] + 4*p->v_offset) / (float)(texHeight<<8)
            );
            glVertex3f(x*SCALE, y*SCALE, z*SCALE);
        }
        // Draw quad's second triangle if present
        int isQuad = p->vertices_indices[3] < (int)vertexCount;
        if (isQuad) {
            int ord[3] = {0,2,3};
            for (int v=0; v<3; v++) {
                int vi = p->vertices_indices[ord[v]];
                int16_t *p0 = animationFrames[f0*vertexCount + vi].xyz;
                int16_t *p2 = animationFrames[f1*vertexCount + vi].xyz;
                float x = (1-alpha)*p0[0] + alpha*p2[0];
                float y = (1-alpha)*p0[1] + alpha*p2[1];
                float z = (1-alpha)*p0[2] + alpha*p2[2];
                glTexCoord2f(
                    p->uv[ord[v]][0] / (float)(texWidth<<8),
                    (p->uv[ord[v]][1] + 4*p->v_offset) / (float)(texHeight<<8)
                );
                glVertex3f(x*SCALE, y*SCALE, z*SCALE);
            }
        }
    }
    glEnd();

    if (overlayEnabled) drawOverlay();
    glutSwapBuffers();
}

// Handle window resize
void reshape(int w, int h) {
    winWidth = w;
    winHeight = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (float)w / h, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <model.car>\n", argv[0]);
        return 1;
    }
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(winWidth, winHeight);
    glutCreateWindow("Chasm The Rift CAR Model Viewer v1.9.0 by SMR9000");

    // Initialize OpenGL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    load_palette_embedded();
    load_car_model(argv[1]);

    // Register callbacks
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