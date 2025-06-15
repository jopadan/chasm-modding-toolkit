// x86_64-w64-mingw32-gcc -std=c11 -O2 objtool100.c -I. -o objtool.exe objtool.res
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>

#ifdef _WIN32
  #include <direct.h>
  #define MKDIR(d) _mkdir(d)
  #define PATHSEP "\\"
#else
  #define MKDIR(d) mkdir(d,0755)
  #define PATHSEP "/"
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#pragma pack(push,1)
typedef struct {
    uint16_t size_x, size_y, x_center;
} FrameHeader;
#pragma pack(pop)

// load 256-color Chasm palette
static uint8_t palette[256][3];
static int load_palette(void) {
    FILE *f = fopen("chasmpalette.act","rb");
    if (!f) { fprintf(stderr,"ERROR: missing chasmpalette.act\n"); return 0; }
    if (fread(palette,3,256,f) != 256) {
        fprintf(stderr,"ERROR: bad palette size\n");
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

// natural order comparator for strings containing numbers
static int natcmp(const void *a, const void *b) {
    const char *s1 = *(const char **)a;
    const char *s2 = *(const char **)b;
    while (*s1 && *s2) {
        if (isdigit((unsigned char)*s1) && isdigit((unsigned char)*s2)) {
            long n1 = strtol(s1, (char**)&s1, 10);
            long n2 = strtol(s2, (char**)&s2, 10);
            if (n1 != n2) return n1 < n2 ? -1 : 1;
        } else {
            char c1 = tolower((unsigned char)*s1++);
            char c2 = tolower((unsigned char)*s2++);
            if (c1 != c2) return c1 < c2 ? -1 : 1;
        }
    }
    return *s1 ? 1 : (*s2 ? -1 : 0);
}

// ---------------------------------------------------------------------------
// -export <sprite.obj>
// (unchanged from objtool09-FINAL.c)
// ---------------------------------------------------------------------------
static int do_export(const char *obj_path) {
    const char *s1=strrchr(obj_path,'/'),*s2=strrchr(obj_path,'\\');
    const char *name = s1? s1+1 : (s2? s2+1 : obj_path);
    char base[512]; strncpy(base,name,sizeof(base)); base[511]=0;
    char *dot=strrchr(base,'.'); if(dot)*dot=0;
    MKDIR(base);

    char manifest[600];
    snprintf(manifest,sizeof(manifest),"%s.txt",base);
    FILE *mf=fopen(manifest,"w");
    if(!mf){perror("fopen manifest");return 1;}
    if(!load_palette())return 1;

    FILE *f=fopen(obj_path,"rb");
    if(!f){perror("fopen OBJ");return 1;}

    uint16_t fc;
    if(fread(&fc,sizeof(fc),1,f)!=1){
        fprintf(stderr,"ERROR reading frame count\n"); fclose(f); return 1;
    }

    for(uint16_t i=0;i<fc;i++){
        FrameHeader h;
        if(fread(&h,sizeof(h),1,f)!=1){
            fprintf(stderr,"ERROR reading header %u\n",i); break;
        }
        unsigned w=h.size_x, H=h.size_y;
        size_t np=(size_t)w*H;
        uint8_t *raw=malloc(np);
        fread(raw,1,np,f);

        uint8_t *rgb=malloc(np*3);
        for(unsigned y=0;y<H;y++)for(unsigned x=0;x<w;x++){
            size_t src=(H-1-y)+(size_t)x*H;
            uint8_t c=raw[src];
            size_t dst=(y*w+x)*3;
            rgb[dst+0]=palette[c][0];
            rgb[dst+1]=palette[c][1];
            rgb[dst+2]=palette[c][2];
        }
        uint8_t *r2=malloc(np*3);
        for(unsigned y=0;y<H;y++)for(unsigned x=0;x<w;x++){
            size_t s=(y*w+x)*3;
            size_t d=((H-1-y)*w+(w-1-x))*3;
            r2[d+0]=rgb[s+0]; r2[d+1]=rgb[s+1]; r2[d+2]=rgb[s+2];
        }

        char png[700];
        snprintf(png,sizeof(png),"%s%s%s_%u.png",
                 base,PATHSEP,base,(unsigned)i);
        stbi_write_png(png,w,H,3,r2,w*3);
        printf("Wrote %s\n",png);

        fprintf(mf,"%s%s%s_%u.png,%u,%u,%u\n",
                base,PATHSEP,base,(unsigned)i,
                w,H,(unsigned)h.x_center);

        free(raw); free(rgb); free(r2);
    }
    fclose(mf);
    fclose(f);
    printf("Wrote manifest %s\n",manifest);
    return 0;
}

// ---------------------------------------------------------------------------
// -dummy <w> <h> <origin> <frames> <palette_idx>
// (unchanged)
// ---------------------------------------------------------------------------
static int do_dummy(int w,int H,int o,int frames,int pidx){
    if(!load_palette())return 1;
    if(pidx<0||pidx>255){fprintf(stderr,"Invalid palette idx\n");return 1;}
    if(frames<1){fprintf(stderr,"Must have >=1 frame\n");return 1;}

    FILE *f=fopen("blank.obj","wb");
    if(!f){perror("fopen blank.obj");return 1;}
    uint16_t cnt=(uint16_t)frames; fwrite(&cnt,2,1,f);

    FrameHeader hdr={(uint16_t)w,(uint16_t)H,(uint16_t)o};
    size_t np=(size_t)w*H;
    uint8_t *buf=malloc(np);
    memset(buf,(uint8_t)pidx,np);
    for(int i=0;i<frames;i++){
        fwrite(&hdr,sizeof(hdr),1,f);
        fwrite(buf,1,np,f);
    }
    free(buf); fclose(f);
    printf("Wrote blank.obj (%dx%d, origin=%d, %d frames, idx=%d)\n",
           w,H,o,frames,pidx);

    MKDIR("blank");
    uint8_t *rgb=malloc(np*3);
    for(size_t p=0;p<np;p++){
        rgb[p*3+0]=palette[pidx][0];
        rgb[p*3+1]=palette[pidx][1];
        rgb[p*3+2]=palette[pidx][2];
    }
    for(int i=0;i<frames;i++){
        char path[200];
        snprintf(path,sizeof(path),"blank%sblank_%d.png",PATHSEP,i);
        stbi_write_png(path,w,H,3,rgb,w*3);
        printf("Wrote %s\n",path);
    }
    free(rgb);

    FILE *mf=fopen("blank.txt","w");
    for(int i=0;i<frames;i++){
        fprintf(mf,"blank%sblank_%d.png,%d,%d,%d\n",
                PATHSEP,i,w,H,o);
    }
    fclose(mf);
    printf("Wrote blank.txt manifest\n");
    return 0;
}

// ---------------------------------------------------------------------------
// -create <manifest.txt> <new.obj>
// (unchanged)
// ---------------------------------------------------------------------------
static int do_create(const char *manifest,const char *outpath){
    if(!load_palette())return 1;
    FILE *mf=fopen(manifest,"r");
    if(!mf){perror("fopen manifest");return 1;}

    typedef struct{ char fn[512]; unsigned w,H,o; } E;
    E *ents=NULL; size_t n=0;
    char line[1024];
    while(fgets(line,sizeof(line),mf)){
        char *p=line+strlen(line)-1;
        while(p>=line&&(*p=='\r'||*p=='\n'))*p--=0;
        if(!*line)continue;
        E e;
        if(sscanf(line,"%511[^,],%u,%u,%u",
                  e.fn,&e.w,&e.H,&e.o)!=4)
        {
            fprintf(stderr,"Bad line: %s\n",line);
            continue;
        }
        ents=realloc(ents,(n+1)*sizeof*ents);
        ents[n++]=e;
    }
    fclose(mf);

    FILE *out=fopen(outpath,"wb");
    if(!out){perror("fopen new.obj");free(ents);return 1;}
    uint16_t cnt=(uint16_t)n; fwrite(&cnt,2,1,out);

    for(size_t i=0;i<n;i++){
        FrameHeader h={(uint16_t)ents[i].w,
                       (uint16_t)ents[i].H,
                       (uint16_t)ents[i].o};
        fwrite(&h,sizeof(h),1,out);

        int iw,ih,ic;
        uint8_t *img =
          stbi_load(ents[i].fn,&iw,&ih,&ic,3);
        if(!img||iw!=(int)ents[i].w||
           ih!=(int)ents[i].H)
        {
            fprintf(stderr,"Fail load %s\n",ents[i].fn);
            if(img)stbi_image_free(img);
            free(ents); fclose(out);
            return 1;
        }
        size_t np=(size_t)iw*ih;
        uint8_t *raw=malloc(np);
        for(unsigned y=0;y<ents[i].H;y++){
            for(unsigned x=0;x<ents[i].w;x++){
              uint8_t r=img[(y*ents[i].w+x)*3+0],
                      g=img[(y*ents[i].w+x)*3+1],
                      b=img[(y*ents[i].w+x)*3+2],
                      best=0;
              for(int pi=0;pi<256;pi++){
                if(palette[pi][0]==r&&
                   palette[pi][1]==g&&
                   palette[pi][2]==b)
                {
                  best=(uint8_t)pi;break;
                }
              }
              raw[y+(size_t)x*ents[i].H]=best;
            }
        }
        stbi_image_free(img);
        fwrite(raw,1,np,out);
        free(raw);
        printf("Imported %s\n",ents[i].fn);
    }
    free(ents);
    fclose(out);
    printf("Wrote new OBJ: %s (%u frames)\n",
           outpath,(unsigned)cnt);
    return 0;
}

// ---------------------------------------------------------------------------
// -manifest <folder>
//                                                                     -----------
static int do_manifest(const char *folder) {
    DIR *d = opendir(folder);
    if (!d) { perror("opendir"); return 1; }
    char **names = NULL; size_t n = 0;
    struct dirent *ent;
    while ((ent = readdir(d))) {
        const char *fn = ent->d_name;
        size_t L = strlen(fn);
        if (L > 4 && !strcasecmp(fn+L-4,".png")) {
            names = realloc(names,(n+1)*sizeof*names);
            names[n++] = strdup(fn);
        }
    }
    closedir(d);
    if (n==0) { fprintf(stderr,"No PNGs in %s\n",folder); return 1; }
    qsort(names,n,sizeof*names,natcmp);

    // output file <folder>.txt next to folder
    char manifest[600];
    snprintf(manifest,sizeof(manifest),"%s.txt",folder);
    FILE *mf = fopen(manifest,"w");
    if (!mf) { perror("fopen manifest"); return 1; }

    for (size_t i=0;i<n;i++) {
        char path[600];
        snprintf(path,sizeof(path),"%s" PATHSEP "%s",folder,names[i]);
        int W,H,ic;
        uint8_t *img = stbi_load(path,&W,&H,&ic,3);
        if (!img) {
            fprintf(stderr,"Failed load %s\n",path);
            continue;
        }
        stbi_image_free(img);
        ic = W/2;  // origin = half width
        fprintf(mf,"%s,%d,%d,%d\n",path,W,H,ic);
        free(names[i]);
    }
    free(names);
    fclose(mf);
    printf("Wrote manifest %s\n",manifest);
    return 0;
}

// ---------------------------------------------------------------------------
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr,
          "Usage:\n"
          "  %s -export   <sprite.obj>\n"
          "  %s -dummy    <w> <h> <origin> <frames> <palette_idx>\n"
          "  %s -create   <manifest.txt> <new.obj>\n"
          "  %s -manifest <folder>\n",
          argv[0],argv[0],argv[0],argv[0]);
        return 1;
    }
    if (!strcmp(argv[1],"-export") && argc==3)
        return do_export(argv[2]);
    if (!strcmp(argv[1],"-dummy") && argc==7)
        return do_dummy(
          atoi(argv[2]),atoi(argv[3]),
          atoi(argv[4]),atoi(argv[5]),
          atoi(argv[6])
        );
    if (!strcmp(argv[1],"-create") && argc==4)
        return do_create(argv[2],argv[3]);
    if (!strcmp(argv[1],"-manifest") && argc==3)
        return do_manifest(argv[2]);

    fprintf(stderr,"Unknown command or wrong args\n");
    return 1;
}
