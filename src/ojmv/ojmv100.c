// omv110.c - Enhanced OBJ Viewer with 2D Texture Preview (v112)
// Compile: x86_64-w64-mingw32-gcc -std=c99 -O2 -I. -L./lib -o omv.exe omv110.c -lfreeglut -lopengl32 -lglu32

#ifdef _WIN32
#  include <windows.h>
#endif

#ifdef MAX_PATH
#  undef MAX_PATH
#endif
#define MAX_PATH 512

#include <stdbool.h>
#include <GL/freeglut.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

// --- Types ---
typedef struct { double x,y,z; } Vertex;
typedef struct { double u,v;   } TexCoord;
typedef struct { unsigned v[4], t[4]; int count; } Face;
struct Camera { float phi, theta; };

// --- Forward Declarations ---
static void compute_center_and_radius(void);
static void compute_normals(void);

// --- Mesh Data ---
static Vertex   *vertices       = NULL; static size_t vertex_count=0, vertex_cap=0;
static Vertex   *next_vertices  = NULL;
static TexCoord *texcoords      = NULL; static size_t texcoord_count=0, tc_cap=0;
static Face     *faces          = NULL; static size_t face_count=0, face_cap=0;
static double   *vnormals       = NULL;

static double    centerX, centerY, centerZ;
static float     initial_distance = 2.0f;
static struct Camera initial_cam = {0.3f,0.4f};

// --- Animation & Texture ---
static char    **model_paths    = NULL; static size_t model_count=0;
static char      texture_path[MAX_PATH] = {0};
static char      model_name[MAX_PATH] = {0};
static GLuint    texID          = 0;
static size_t    current_frame  = 0;
static int       frame_interval = 100;
static const int update_interval= 16;
static double    interp_factor  = 0.0;
static bool      is_playing     = true;

// --- View State ---
static bool     wireframe_fill    = false;
static bool     wireframe_overlay = false;
static bool     smooth_shade      = true;
static bool     normal_color      = false;
static bool     linear_filter     = false;
static bool     texture_on        = true;
static bool     show_help         = true;
static bool     show_model        = true;
static bool     show_texture_only = false;
static bool     flat_color_mode   = false;

static float    DISTANCE          = 2.0f;
static struct Camera cam = {0.3f,0.4f};
static float    panX=0, panY=0;
static bool     fit_on_load = true;
static int      winW=800, winH=600;
static bool     left_down = false;
static int      last_x, last_y;

// --- Statistics ---
static size_t   triangle_count = 0;
static size_t   quad_count = 0;

// Helper functions
static bool push_vertex(Vertex v) {
    if(vertex_count == vertex_cap) {
        size_t new_cap = vertex_cap ? vertex_cap*2 : 256;
        Vertex *new_v = realloc(vertices, new_cap * sizeof *vertices);
        if(!new_v) return false;
        vertices = new_v;
        vertex_cap = new_cap;
    }
    vertices[vertex_count++] = v;
    return true;
}

static bool push_texcoord(TexCoord t) {
    if(texcoord_count == tc_cap) {
        tc_cap = tc_cap ? tc_cap*2 : 256;
        TexCoord *new_t = realloc(texcoords, tc_cap * sizeof *texcoords);
        if(!new_t) return false;
        texcoords = new_t;
    }
    texcoords[texcoord_count++] = t;
    return true;
}

static bool push_face(Face f) {
    if(face_count == face_cap) {
        face_cap = face_cap ? face_cap*2 : 256;
        Face *new_f = realloc(faces, face_cap * sizeof *faces);
        if(!new_f) return false;
        faces = new_f;
    }
    faces[face_count++] = f;
    return true;
}

static void cleanup(void) {
    free(vertices); vertices = NULL;
    free(next_vertices); next_vertices = NULL;
    free(texcoords); texcoords = NULL;
    free(faces); faces = NULL;
    free(vnormals); vnormals = NULL;
    if(model_paths) {
        for(size_t i=0; i<model_count; i++) free(model_paths[i]);
        free(model_paths); model_paths = NULL;
    }
    if(texID) { glDeleteTextures(1, &texID); texID = 0; }
}

static void load_manifest(const char *json_path) {
    const char *base = strrchr(json_path, '/');
    if(!base) base = strrchr(json_path, '\\');
    if(base) base++;
    else base = json_path;
    
    FILE *f = fopen(json_path,"r");
    if(!f) { perror("fopen manifest"); exit(1); }
    
    char line[1024];
    while(fgets(line,sizeof line,f)) {
        if(strstr(line,"\"type\"") && strstr(line,"\"model\"")) model_count++;
    }
    if(model_count==0) { fprintf(stderr,"No models in manifest\n"); exit(1); }
    
    rewind(f);
    model_paths = malloc(model_count * sizeof *model_paths);
    size_t m=0;
    while(fgets(line,sizeof line,f)) {
        bool isModel = strstr(line,"\"type\"") && strstr(line,"\"model\"");
        bool isTex   = strstr(line,"\"type\"") && strstr(line,"\"texture\"");
        if(isModel||isTex) {
            char *p = strstr(line,"\"path\"");
            if(p) {
                char buf[MAX_PATH];
                char *q = strchr(p+6,'"');
                if(q && sscanf(q+1,"%511[^\"]",buf)==1) {
                    if(isModel) {
                        model_paths[m] = strdup(buf);
                        char* slash = strrchr(buf, '/');
                        if(!slash) slash = strrchr(buf, '\\');
                        if(slash) strncpy(model_name, slash+1, MAX_PATH);
                        else strncpy(model_name, buf, MAX_PATH);
                        char* dot = strrchr(model_name, '.');
                        if(dot) *dot = '\0';
                        m++;
                    }
                    else strncpy(texture_path,buf,MAX_PATH);
                }
            }
        }
    }
    fclose(f);
}

static void load_texture(const char *path) {
    int w,h,comp;
    unsigned char *data = stbi_load(path,&w,&h,&comp,4);
    if(!data) { fprintf(stderr,"Fail tex %s\n",path); return; }
    
    glGenTextures(1,&texID);
    glBindTexture(GL_TEXTURE_2D,texID);
    GLenum filt = linear_filter?GL_LINEAR:GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,filt);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,filt);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,data);
    stbi_image_free(data);
}

static void load_obj(const char *path) {
    vertex_count = texcoord_count = face_count = 0;
    triangle_count = quad_count = 0;
    free(vnormals); vnormals = NULL;
    
    FILE *f = fopen(path,"r");
    if(!f) { perror(path); exit(1); }
    
    char buf[1024];
    while(fgets(buf,sizeof buf,f)) {
        char *tok = strtok(buf," \t\r\n");
        if(!tok) continue;
        
        if(!strcmp(tok,"v")) {
            push_vertex((Vertex){
                atof(strtok(NULL," ")),
                atof(strtok(NULL," ")),
                atof(strtok(NULL," "))
            });
        } else if(!strcmp(tok,"vt")) {
            push_texcoord((TexCoord){
                atof(strtok(NULL," ")),
                atof(strtok(NULL," "))
            });
        } else if(!strcmp(tok,"f")) {
            Face fc={{0},{0},0};
            for(int i=0;i<4;i++) {
                char *p = strtok(NULL," \t\r\n"); if(!p) break;
                fc.count++;
                unsigned vi,ti; sscanf(p,"%u/%u",&vi,&ti);
                fc.v[i] = vi-1; fc.t[i] = ti-1;
            }
            push_face(fc);
            
            if(fc.count == 3) triangle_count++;
            else if(fc.count == 4) quad_count++;
        }
    }
    fclose(f);

    if(fit_on_load) {
        compute_center_and_radius();
        initial_distance = DISTANCE;
        initial_cam = cam;
        fit_on_load = false;
    }
    compute_normals();
}

static void compute_center_and_radius(void) {
    if(vertex_count==0) return;
    
    double minX=vertices[0].x, maxX=minX,
           minY=vertices[0].y, maxY=minY,
           minZ=vertices[0].z, maxZ=minZ;
           
    for(size_t i=1;i<vertex_count;i++) {
        minX=fmin(minX,vertices[i].x); maxX=fmax(maxX,vertices[i].x);
        minY=fmin(minY,vertices[i].y); maxY=fmax(maxY,vertices[i].y);
        minZ=fmin(minZ,vertices[i].z); maxZ=fmax(maxZ,vertices[i].z);
    }
    
    centerX=(minX+maxX)*0.5;
    centerY=(minY+maxY)*0.5;
    centerZ=(minZ+maxZ)*0.5;
    
    double r=0;
    for(size_t i=0;i<vertex_count;i++) {
        double dx=vertices[i].x-centerX,
               dy=vertices[i].y-centerY,
               dz=vertices[i].z-centerZ;
        r = fmax(r, sqrt(dx*dx+dy*dy+dz*dz));
    }
    if(r>0) DISTANCE = (float)(r*2.0);
}

static void compute_normals(void) {
    if(vertex_count==0) return;
    
    vnormals = calloc(vertex_count*3,sizeof *vnormals);
    for(size_t i=0;i<face_count;i++) {
        Face *f = &faces[i];
        for(int t=0;t<f->count-2;t++) {
            unsigned vs[3] = {f->v[0],f->v[t+1],f->v[t+2]};
            Vertex *A=&vertices[vs[0]],*B=&vertices[vs[1]],*C=&vertices[vs[2]];
            double ux=B->x-A->x, uy=B->y-A->y, uz=B->z-A->z;
            double vx=C->x-A->x, vy=C->y-A->y, vz=C->z-A->z;
            double fn[3] = {uy*vz-uz*vy, uz*vx-ux*vz, ux*vy-uy*vx};
            
            for(int k=0;k<3;k++) {
                size_t off = vs[k]*3;
                vnormals[off+0]+=fn[0];
                vnormals[off+1]+=fn[1];
                vnormals[off+2]+=fn[2];
            }
        }
    }
    
    for(size_t i=0;i<vertex_count;i++) {
        double *n = &vnormals[i*3];
        double L = sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
        if(L>1e-6){ n[0]/=L; n[1]/=L; n[2]/=L; }
    }
}

static void timer_cb(int) {
    if(model_count>1 && is_playing) {
        interp_factor += (double)update_interval / frame_interval;
        
        if(interp_factor >= 1.0) {
            interp_factor = 0.0;
            current_frame = (current_frame+1) % model_count;
            
            free(next_vertices);
            load_obj(model_paths[(current_frame+1) % model_count]);
            next_vertices = malloc(vertex_count * sizeof(Vertex));
            memcpy(next_vertices, vertices, vertex_count * sizeof(Vertex));
            load_obj(model_paths[current_frame]);
        }
    }
    glutPostRedisplay();
    glutTimerFunc(update_interval, timer_cb, 0);
}

static void init_gl(void) {
    glClearColor(33/255.0f,33/255.0f,33/255.0f,1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    
    GLfloat amb[]={0.2f,0.2f,0.2f,1}, dif[]={0.8f,0.8f,0.8f,1},
            spec[]={1,1,1,1}, pos[]={1,1,1,0};
    glLightfv(GL_LIGHT0,GL_AMBIENT, amb);
    glLightfv(GL_LIGHT0,GL_DIFFUSE, dif);
    glLightfv(GL_LIGHT0,GL_SPECULAR,spec);
    glLightfv(GL_LIGHT0,GL_POSITION,pos);
}

static void display_cb(void) {
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    if(show_texture_only && texID) {
        // 2D Texture preview mode
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluOrtho2D(0, 1, 0, 1);
        
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texID);
        
        glColor3f(1,1,1);
        glBegin(GL_QUADS);
        glTexCoord2f(0,0); glVertex2f(0,0);
        glTexCoord2f(1,0); glVertex2f(1,0);
        glTexCoord2f(1,1); glVertex2f(1,1);
        glTexCoord2f(0,1); glVertex2f(0,1);
        glEnd();

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glEnable(GL_DEPTH_TEST);
    }
    else if(show_model) {
        // 3D Model rendering
        float cx=DISTANCE*cosf(cam.phi)*sinf(cam.theta),
              cy=DISTANCE*sinf(cam.phi),
              cz=DISTANCE*cosf(cam.theta);
        gluLookAt(cx,cy,cz,0,0,0,0,1,0);

        glTranslatef(panX,panY,0);
        glTranslatef(-centerX,-centerY,-centerZ);

        if(flat_color_mode) {
            glDisable(GL_LIGHTING);
            glDisable(GL_TEXTURE_2D);
            glShadeModel(GL_FLAT);
        } else if(normal_color) {
            glDisable(GL_LIGHTING);
            glDisable(GL_TEXTURE_2D);
        } else {
            glEnable(GL_LIGHTING);
            if(texID){ 
                glEnable(GL_TEXTURE_2D); 
                glBindTexture(GL_TEXTURE_2D,texID); 
            }
            else glDisable(GL_TEXTURE_2D);
        }

        glShadeModel(smooth_shade?GL_SMOOTH:GL_FLAT);
        glPolygonMode(GL_FRONT_AND_BACK, wireframe_fill?GL_LINE:GL_FILL);

        if(!show_texture_only) {
            for(size_t i=0;i<face_count;i++) {
                Face *f=&faces[i];
                GLenum prim = (f->count==3?GL_TRIANGLES:GL_QUADS);
                glBegin(prim);
                for(int k=0;k<f->count;k++) {
                    unsigned vi=f->v[k], ti=f->t[k];
                    double *n = &vnormals[3*vi];
                    
                    if(flat_color_mode) {
                        if(f->count == 3) glColor3f(1,0,0);
                        else glColor3f(0,0,1);
                    } else if(normal_color) {
                        glColor3d((n[0]+1)*0.5, (n[1]+1)*0.5, (n[2]+1)*0.5);
                    } else {
                        glNormal3dv(n);
                        if(texture_on && ti<texcoord_count) {
                            TexCoord *t=&texcoords[ti];
                            glTexCoord2d(t->u,1.0-t->v);
                        }
                        glColor3f(1,1,1);
                    }
                    
                    Vertex *V=&vertices[vi];
                    if(next_vertices && model_count>1) {
                        Vertex *Vnext = &next_vertices[vi];
                        double x = V->x + (Vnext->x - V->x) * interp_factor;
                        double y = V->y + (Vnext->y - V->y) * interp_factor;
                        double z = V->z + (Vnext->z - V->z) * interp_factor;
                        glVertex3d(x,y,z);
                    } else {
                        glVertex3d(V->x,V->y,V->z);
                    }
                }
                glEnd();
            }
        }

        if(wireframe_overlay && !show_texture_only) {
            glEnable(GL_POLYGON_OFFSET_LINE);
            glPolygonOffset(-1,-1);
            glDisable(GL_LIGHTING);
            glDisable(GL_TEXTURE_2D);
            glColor3f(0,0,0);
            glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
            for(size_t i=0;i<face_count;i++) {
                Face *f=&faces[i];
                GLenum prim=(f->count==3?GL_TRIANGLES:GL_QUADS);
                glBegin(prim);
                for(int k=0;k<f->count;k++) {
                    Vertex *V=&vertices[f->v[k]];
                    if(next_vertices && model_count>1) {
                        Vertex *Vnext = &next_vertices[f->v[k]];
                        double x = V->x + (Vnext->x - V->x) * interp_factor;
                        double y = V->y + (Vnext->y - V->y) * interp_factor;
                        double z = V->z + (Vnext->z - V->z) * interp_factor;
                        glVertex3d(x,y,z);
                    } else {
                        glVertex3d(V->x,V->y,V->z);
                    }
                }
                glEnd();
            }
            glPolygonMode(GL_FRONT_AND_BACK, wireframe_fill?GL_LINE:GL_FILL);
            if(!normal_color && !flat_color_mode){
                glEnable(GL_LIGHTING);
                if(texID) glEnable(GL_TEXTURE_2D);
            }
            glDisable(GL_POLYGON_OFFSET_LINE);
        }
    }

    if(show_help) {
        const char *controls[] = {
            "",
            "VIEW CONTROLS:",
            "W/S: zoom in/out",
            "A/D: rotate left/right",
            "ARROWS: pan",
            "MOUSE DRAG: rotate",
            "",
            "DISPLAY MODES:",
            "TAB: toggle wireframe",
            "O: wireframe overlay",
            "T: show texture",
            "F5: flat shading",
            "F6: normal color",
            "F11: flat color mode",
            "F: texture filter",
            "",
            "ANIMATION:",
            "SPACE: play/pause",
            "Q: previous frame",
            "E: next frame",
            "+/-: speed up/down",
            "R: reset view",
            "F1: hide info text"
        };
        
        char model_info[128];
        snprintf(model_info, sizeof(model_info), "%s.obj", model_name);
        
        char stats_info[256];
        snprintf(stats_info, sizeof(stats_info), 
            "POLYGONS: %zu (TRI: %zu, QUAD: %zu)\nVERTICES: %zu",
            face_count, triangle_count, quad_count, vertex_count);
        
        const char *info[] = {
            "MODEL INFO:",
            model_info,
            texture_path[0] ? texture_path : "NO TEXTURE",
            model_count > 1 ? "JSON ANIMATION MODE" : "SINGLE MODEL MODE",
            model_count > 1 ? "" : NULL,
            stats_info
        };
        
        if(model_count > 1) {
            char frame_info[64];
            snprintf(frame_info, sizeof(frame_info), "FRAME: %zu/%zu", current_frame+1, model_count);
            info[4] = frame_info;
        }

        glMatrixMode(GL_PROJECTION);
        glPushMatrix(); glLoadIdentity();
        gluOrtho2D(0,winW,0,winH);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix(); glLoadIdentity();
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        
        glColor3f(0.8f,0.8f,1.0f);
        int line_height = 14;
        int start_y = winH - line_height;
        
        for(int i=0; i<sizeof(controls)/sizeof(controls[0]); i++) {
            if(controls[i][0] == '\0') {
                start_y -= line_height/2;
                continue;
            }
            glRasterPos2i(10, start_y);
            for(const char *c=controls[i]; *c; ++c) {
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);
            }
            start_y -= line_height;
        }
        
        glColor3f(0.8f,1.0f,0.8f);
        start_y -= line_height*2;
        
        for(int i=0; i<sizeof(info)/sizeof(info[0]); i++) {
            if(info[i] == NULL) continue;
            if(i == 5) {
                char *line = strtok((char*)info[i], "\n");
                while(line) {
                    glRasterPos2i(10, start_y);
                    for(const char *c=line; *c; ++c) {
                        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);
                    }
                    start_y -= line_height;
                    line = strtok(NULL, "\n");
                }
            } else {
                glRasterPos2i(10, start_y);
                for(const char *c=info[i]; *c; ++c) {
                    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);
                }
                start_y -= line_height;
            }
        }

        glMatrixMode(GL_PROJECTION); glPopMatrix();
        glMatrixMode(GL_MODELVIEW);  glPopMatrix();
    }

    glutSwapBuffers();
}

static void reshape_cb(int w,int h) {
    winW=w; winH=h;
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(60.0,(h?(double)w/h:1.0),0.1,1000.0);
    glMatrixMode(GL_MODELVIEW);
}

static void keyboard_cb(unsigned char key,int x,int y) {
    (void)x;(void)y;
    switch(toupper(key)){
      case 27: cleanup(); exit(0);
      case '\t': wireframe_fill = !wireframe_fill; break;
      case 'O': wireframe_overlay = !wireframe_overlay; break;
      case 'W': DISTANCE = fmaxf(0.1f,DISTANCE-0.5f); break;
      case 'S': DISTANCE += 0.5f; break;
      case 'A': cam.theta -= 0.05f; break;
      case 'D': cam.theta += 0.05f; break;
      case '+': frame_interval = frame_interval>update_interval
                                ? frame_interval-update_interval
                                : update_interval; break;
      case '-': frame_interval += update_interval; break;
      case 'F': 
                linear_filter = !linear_filter;
                if(texID){
                    glBindTexture(GL_TEXTURE_2D,texID);
                    GLenum filt=linear_filter?GL_LINEAR:GL_NEAREST;
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,filt);
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,filt);
                }
                break;
      case 'T': 
                show_texture_only = !show_texture_only;
                if(show_texture_only) {
                    show_model = false;
                } else {
                    show_model = true;
                }
                break;
      case ' ': is_playing = !is_playing; break;
      case 'Q':
                if(model_count > 1) {
                    is_playing = false;
                    interp_factor = 0.0;
                    current_frame = (current_frame == 0) ? model_count-1 : current_frame-1;
                    free(next_vertices);
                    load_obj(model_paths[current_frame]);
                    if(model_count > 1) {
                        size_t next_frame = (current_frame + 1) % model_count;
                        load_obj(model_paths[next_frame]);
                        next_vertices = malloc(vertex_count * sizeof(Vertex));
                        memcpy(next_vertices, vertices, vertex_count * sizeof(Vertex));
                        load_obj(model_paths[current_frame]);
                    }
                }
                break;
      case 'E':
                if(model_count > 1) {
                    is_playing = false;
                    interp_factor = 0.0;
                    current_frame = (current_frame + 1) % model_count;
                    free(next_vertices);
                    load_obj(model_paths[current_frame]);
                    if(model_count > 1) {
                        size_t next_frame = (current_frame + 1) % model_count;
                        load_obj(model_paths[next_frame]);
                        next_vertices = malloc(vertex_count * sizeof(Vertex));
                        memcpy(next_vertices, vertices, vertex_count * sizeof(Vertex));
                        load_obj(model_paths[current_frame]);
                    }
                }
                break;
      case 'R':
                DISTANCE = initial_distance;
                cam = initial_cam;
                panX = 0;
                panY = 0;
                break;
      default: break;
    }
    glutPostRedisplay();
}

static void special_cb(int key,int x,int y) {
    (void)x;(void)y;
    switch(key){
      case GLUT_KEY_UP:    panY += 0.1f; break;
      case GLUT_KEY_DOWN:  panY -= 0.1f; break;
      case GLUT_KEY_LEFT:  panX -= 0.1f; break;
      case GLUT_KEY_RIGHT: panX += 0.1f; break;
      case GLUT_KEY_F1:    show_help = !show_help; break;
      case GLUT_KEY_F5:    smooth_shade = !smooth_shade; break;
      case GLUT_KEY_F6:    normal_color = !normal_color; break;
      case GLUT_KEY_F11:   flat_color_mode = !flat_color_mode; break;
      default: break;
    }
    glutPostRedisplay();
}

static void mouse_cb(int b,int s,int x,int y) {
    if(b==GLUT_LEFT_BUTTON){
        left_down = (s==GLUT_DOWN);
        last_x = x; last_y = y;
    }
}

static void motion_cb(int x,int y) {
    if(!left_down) return;
    int dx = x - last_x, dy = y - last_y;
    cam.theta += dx*0.01f;
    cam.phi   += dy*0.01f;
    last_x = x; last_y = y;
    glutPostRedisplay();
}

int main(int argc,char **argv) {
    atexit(cleanup);
    
    if(argc<2){
        fprintf(stderr,"Usage: %s <model.obj>|<manifest.json>\n",argv[0]);
        return 1;
    }
    
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA|GLUT_DEPTH);
    glutInitWindowSize(winW,winH);
    glutCreateWindow("Enhanced OBJ + JSON Model Viewer v1.0.0 by SMR9000");

    init_gl();
    glutDisplayFunc(display_cb);
    glutReshapeFunc(reshape_cb);
    glutKeyboardFunc(keyboard_cb);
    glutSpecialFunc(special_cb);
    glutMouseFunc(mouse_cb);
    glutMotionFunc(motion_cb);

    const char *arg = argv[1];
    size_t L = strlen(arg);
    if(L>5 && strcmp(arg+L-5,".json")==0){
        load_manifest(arg);
        if(texture_path[0]) load_texture(texture_path);
        load_obj(model_paths[0]);
        
        if(model_count > 1) {
            load_obj(model_paths[1]);
            next_vertices = malloc(vertex_count * sizeof(Vertex));
            memcpy(next_vertices, vertices, vertex_count * sizeof(Vertex));
            load_obj(model_paths[0]);
        }
        
        glutTimerFunc(frame_interval,timer_cb,0);
    } else {
        char* slash = strrchr(arg, '/');
        if(!slash) slash = strrchr(arg, '\\');
        if(slash) strncpy(model_name, slash+1, MAX_PATH);
        else strncpy(model_name, arg, MAX_PATH);
        char* dot = strrchr(model_name, '.');
        if(dot) *dot = '\0';
        load_obj(arg);
    }

    glutMainLoop();
    return 0;
}