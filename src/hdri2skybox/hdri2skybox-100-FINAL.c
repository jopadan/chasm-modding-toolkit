// hdri2skybox.c
// x86_64-w64-mingw32-gcc -std=c11     -I. -I./stb     hdri2skybox.c     -L./lib -lfreeglut -lglu32 -lopengl32 -lm     -o hdri2skybox.exe hdri2skybox.res
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
  #include <direct.h>
  #define MKDIR(p) _mkdir(p)
#else
  #include <sys/stat.h>
  #include <sys/types.h>
  #define MKDIR(p) mkdir(p, 0755)
#endif
#include <GL/freeglut.h>           // just to create a GL context
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// normalize a 3D vector in-place
static void normalize3(float v[3]) {
    float len = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (len > 0.0f) {
        v[0] /= len;  v[1] /= len;  v[2] /= len;
    }
}

// nearest-neighbor sample from equirectangular map
static unsigned char fetchEquirect(
    const unsigned char *img, int W, int H, int C,
    float u, float v, int ch)
{
    int x = (int)(u*(W-1) + 0.5f),
        y = (int)(v*(H-1) + 0.5f);
    if (x<0) x=0; else if (x>=W) x=W-1;
    if (y<0) y=0; else if (y>=H) y=H-1;
    return img[(y*W + x)*C + ch];
}

// pick faceSize = highest power-of-two ≤ min(W/4, H/2)
static int deriveFaceSize(int W, int H) {
    int m = W/4 < H/2 ? W/4 : H/2;
    int s = 1;
    while ((s<<1) <= m) s <<= 1;
    return s;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, 
            "HDRI to SKYBOX Converter v1.0.0 by SMR9000\n\n"
            "Usage: %s <input.jpg/png> [faceSize]\n\n"
            "Use https://www.manyworlds.run to create HDRI skybox\n"
            "This Tool uses only HDRI images in PNG/JPG as input\n"
            "Drag and drop image on executable to autogenerate size\n"
            
            , argv[0]);
        return 1;
    }
    const char *infile = argv[1];
    int manual = (argc==3), faceSize = manual ? atoi(argv[2]) : 0;

    // load the panorama first so we can auto-derive size if needed
    int W,H,C;
    unsigned char *pan = stbi_load(infile, &W,&H,&C, 3);
    if (!pan) {
        fprintf(stderr, "Error: failed to load “%s”\n", infile);
        return 1;
    }
    if (!manual) {
        faceSize = deriveFaceSize(W,H);
        if (faceSize < 2) {
            fprintf(stderr, "Error: panorama too small ( %dx%d )\n", W,H);
            stbi_image_free(pan);
            return 1;
        }
        printf("Auto-derived faceSize = %d\n", faceSize);
    }

    // extract just the filename (handles both “/” and “\\” paths)
    char base[256];
    {
        const char *s1 = strrchr(infile, '/'),
                   *s2 = strrchr(infile, '\\');
        const char *fn = infile;
        if (s1 && s2) fn = (s1 > s2 ? s1 : s2) + 1;
        else if (s1) fn = s1+1;
        else if (s2) fn = s2+1;
        strncpy(base, fn, sizeof(base)-1);
        base[sizeof(base)-1] = '\0';
        char *dot = strrchr(base, '.');
        if (dot) *dot = '\0';
    }

    // make the output directory
    if (MKDIR(base) != 0 && errno != EEXIST) {
        perror("mkdir");
        stbi_image_free(pan);
        return 1;
    }

    // tiny hidden GLUT context
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA|GLUT_DOUBLE|GLUT_DEPTH);
    glutInitWindowSize(1,1);
    glutCreateWindow("HDRI to SKYBOX Converter v1.0.0 by SMR9000");

    // buffer for one face
    unsigned char *face = malloc(faceSize*faceSize*3);
    if (!face) {
        fprintf(stderr, "Error: out of memory\n");
        stbi_image_free(pan);
        return 1;
    }

    // swap .cel.0 <-> .cel.1 in the filename map
    int faceMap[6] = { 1, 0, 2, 3, 4, 5 };

    // generate all 6 faces
    for (int f = 0; f < 6; ++f) {
        for (int y = 0; y < faceSize; ++y) {
            for (int x = 0; x < faceSize; ++x) {
                // NDC → [-1..1]
                float U = 2.0f*(x+0.5f)/faceSize - 1.0f;
                float V = 2.0f*(y+0.5f)/faceSize - 1.0f;
                // build direction
                float d[3];
                switch(f) {
                  case 0: d[0]=-1; d[1]=-V; d[2]= U; break; // left
                  case 1: d[0]=+1; d[1]=-V; d[2]=-U; break; // right
                  case 2: d[0]= U; d[1]=+1; d[2]= V; break; // top
                  case 3: d[0]= U; d[1]=-1; d[2]=-V; break; // bottom
                  case 4: d[0]= U; d[1]=-V; d[2]=+1; break; // front
                  default:d[0]=-U; d[1]=-V; d[2]=-1; break; // back
                }
                normalize3(d);

                // spherical coords
                float theta = atan2f(d[2], d[0]);
                float phi   = asinf(d[1]);
                float eu = (theta + M_PI)/(2.0f*M_PI);
                float ev = (phi   + M_PI/2.0f)/M_PI;
                // flip V for image row-0=top
                float sampleV = 1.0f - ev;

                for (int c = 0; c < 3; ++c) {
                    face[(y*faceSize + x)*3 + c] =
                      fetchEquirect(pan, W,H,3, eu, sampleV, c);
                }
            }
        }

        // determine output index and path
        int outF = faceMap[f];
        char path[512];
        snprintf(path, sizeof(path),
                 "%s/%s.cel.%d.png", base, base, outF);

        if (!stbi_write_png(path, faceSize, faceSize, 3, face, faceSize*3)) {
            fprintf(stderr, "Error: failed to write “%s”\n", path);
        } else {
            printf("Wrote: %s\n", path);
        }
    }

    free(face);
    stbi_image_free(pan);
    return 0;
}
