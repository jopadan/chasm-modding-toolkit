// x86_64-w64-mingw32-gcc -std=c99 -O2 -o pal2all.exe pal2all104-Final.c pal2all.res
// --------------------------------------------------
// Verion 1.0.0 (104)
// Reads a 768-byte .PAL and emits into ./ChasmPalette/:
//   Photoshop_<base>_transparent.act   (scaled 6→8-bit if needed)
//   Photoshop_<base>_pink.act          (index 255 = #FC00C8)
//   Quake_<base>.lmp                   (raw 768 bytes, pink palette)
//   Jasc_<base>.pal                    (JASC-PAL ASCII, pink palette)
//   PNG_<base>.png                     (16×16 indexed PNG, pink palette)
//   Gimp_<base>.gpl                    (GIMP palette, pink palette)
//   Code_<base>.h                      (C header: static const uint8_t <base>_palette[256][3])
//   Cube_<base>.cube                   (3D LUT, size=16, maps to original palette)
// Usage:
//   pal2all.exe input.pal
// --------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
  #include <direct.h>
  #define MKDIR(dir) _mkdir(dir)
#else
  #include <sys/stat.h>
  #define MKDIR(dir) mkdir(dir,0755)
#endif

//—— PNG helper routines ————————————————————————————————————————
static uint32_t crc_table[256]; static int crc_ready=0;
static void make_crc_table(void){
    for(int n=0;n<256;n++){
        uint32_t c=(uint32_t)n;
        for(int k=0;k<8;k++)
            c=(c&1)?(0xEDB88320u^(c>>1)):(c>>1);
        crc_table[n]=c;
    }
    crc_ready=1;
}
static uint32_t update_crc(uint32_t crc,const uint8_t *buf,size_t len){
    if(!crc_ready) make_crc_table();
    crc^=0xFFFFFFFFu;
    for(size_t i=0;i<len;i++)
        crc=crc_table[(crc^buf[i])&0xFF]^(crc>>8);
    return crc^0xFFFFFFFFu;
}
static uint32_t adler32(const uint8_t *data,size_t len){
    const uint32_t MOD=65521;
    uint32_t s1=1,s2=0;
    for(size_t i=0;i<len;i++){
        s1=(s1+data[i])%MOD;
        s2=(s2+s1)%MOD;
    }
    return (s2<<16)|s1;
}
static void w32(FILE*f,uint32_t v){
    fputc((v>>24)&0xFF,f);
    fputc((v>>16)&0xFF,f);
    fputc((v>>8 )&0xFF,f);
    fputc((v    )&0xFF,f);
}
static void write_chunk(FILE*f,const char*t,const uint8_t*d,uint32_t len){
    w32(f,len);
    fwrite(t,1,4,f);
    if(len) fwrite(d,1,len,f);
    uint8_t *buf=malloc(4+len);
    memcpy(buf,t,4);
    if(len) memcpy(buf+4,d,len);
    uint32_t crc=update_crc(0,buf,4+len);
    free(buf);
    w32(f,crc);
}

//—— Write 16×16 indexed PNG from pal[256][3] ——————————————————————
static void write_png(const char *fn,uint8_t pal[256][3]){
    const int W=16,H=16;
    size_t raw_len=H*(1+W);
    uint8_t *raw=malloc(raw_len);
    for(int y=0;y<H;y++){
        raw[y*(W+1)] = 0; 
        for(int x=0;x<W;x++)
            raw[y*(W+1)+1+x] = (uint8_t)(y*W + x);
    }
    uint8_t zhdr[2]={0x78,0x01};
    uint16_t blen=(uint16_t)raw_len;
    uint8_t dhdr[5]={0x01,
        (uint8_t)(blen&0xFF),(uint8_t)((blen>>8)&0xFF),
        (uint8_t)(~blen&0xFF),(uint8_t)((~blen>>8)&0xFF)
    };
    uint32_t adl=adler32(raw,raw_len);
    size_t idat_len=2+5+raw_len+4;
    uint8_t *idat=malloc(idat_len);
    size_t off=0;
    memcpy(idat+off,zhdr,2); off+=2;
    memcpy(idat+off,dhdr,5); off+=5;
    memcpy(idat+off,raw, raw_len); off+=raw_len;
    idat[off++]=(adl>>24)&0xFF; idat[off++]=(adl>>16)&0xFF;
    idat[off++]=(adl>>8 )&0xFF; idat[off++]=adl&0xFF;

    FILE *f=fopen(fn,"wb"); 
    if(!f){ perror("PNG fopen"); exit(1); }
    const uint8_t sig[8]={137,80,78,71,13,10,26,10};
    fwrite(sig,1,8,f);
    uint8_t ihdr[13]={0,0,0,W,0,0,0,H,8,3,0,0,0};
    write_chunk(f,"IHDR",ihdr,13);
    write_chunk(f,"PLTE",&pal[0][0],256*3);
    write_chunk(f,"IDAT",idat,(uint32_t)idat_len);
    write_chunk(f,"IEND",NULL,0);
    fclose(f);
    free(raw); free(idat);
}

//—— Utility: extract basename (no path/ext) ————————————————————————
static void split_base(const char *path,char *out){
    const char *fn=strrchr(path,'/');
  #ifdef _WIN32
    const char *fn2=strrchr(path,'\\');
    if(fn2&&(!fn||fn2>fn)) fn=fn2;
  #endif
    if(fn) fn++; else fn=path;
    const char *dot=strrchr(fn,'.');
    size_t len=dot?(size_t)(dot-fn):strlen(fn);
    memcpy(out,fn,len); out[len]=0;
}
static void die(const char*msg){fprintf(stderr,"%s\n",msg);exit(1);}

int main(int argc,char**argv){
    if(argc!=2){
        fprintf(stderr,"Usage: %s input.pal\n",argv[0]);
        return 1;
    }
    char base[256]; split_base(argv[1],base);
    MKDIR("ChasmPalette");

    // Load and scale palette
    FILE *fin=fopen(argv[1],"rb");
    if(!fin) die("Opening .PAL");
    uint8_t palf[256][3];
    if(fread(palf,1,768,fin)!=768) die(".PAL must be exactly 768 bytes");
    fclose(fin);
    uint8_t mx=0;
    for(int i=0;i<256;i++)
      for(int c=0;c<3;c++)
        if(palf[i][c]>mx) mx=palf[i][c];
    if(mx<=63)
      for(int i=0;i<256;i++)
        for(int c=0;c<3;c++)
          palf[i][c]<<=2;

    // Build pink palette override
    uint8_t pal_pink[256][3];
    memcpy(pal_pink,palf,768);
    pal_pink[255][0]=0xFC; pal_pink[255][1]=0x00; pal_pink[255][2]=0xC8;

    // 1) Photoshop ACTs
    char fn_a1[512], fn_a2[512];
    snprintf(fn_a1,sizeof(fn_a1),"ChasmPalette/Photoshop_%s_transparent.act",base);
    snprintf(fn_a2,sizeof(fn_a2),"ChasmPalette/Photoshop_%s_pink.act",       base);
    { FILE*f=fopen(fn_a1,"wb"); fwrite(palf,1,768,f); fclose(f); }
    { FILE*f=fopen(fn_a2,"wb"); fwrite(pal_pink,1,768,f); fclose(f); }

    // 2) Quake lump
    char fn_q[512];
    snprintf(fn_q,sizeof(fn_q),"ChasmPalette/Quake_%s.lmp",base);
    { FILE*f=fopen(fn_q,"wb"); fwrite(pal_pink,1,768,f); fclose(f); }

    // 3) JASC-PAL
    char fn_j[512];
    snprintf(fn_j,sizeof(fn_j),"ChasmPalette/Jasc_%s.pal",base);
    { FILE*f=fopen(fn_j,"w");
      fprintf(f,"JASC-PAL\n0100\n256\n");
      for(int i=0;i<256;i++)
        fprintf(f,"%d %d %d\n",
                pal_pink[i][0],pal_pink[i][1],pal_pink[i][2]);
      fclose(f);
    }

    // 4) PNG
    char fn_p[512];
    snprintf(fn_p,sizeof(fn_p),"ChasmPalette/PNG_%s.png",base);
    write_png(fn_p,pal_pink);

    // 5) GIMP palette
    char fn_g[512];
    snprintf(fn_g,sizeof(fn_g),"ChasmPalette/Gimp_%s.gpl",base);
    { FILE*f=fopen(fn_g,"w");
      fprintf(f,"GIMP Palette\nName: %s\nColumns: 16\n#\n",base);
      for(int i=0;i<256;i++)
        fprintf(f,"%3d %3d %3d\tColor%03d\n",
                pal_pink[i][0],pal_pink[i][1],pal_pink[i][2],i);
      fclose(f);
    }

    // 6) C header
    char guard[300];
    { size_t L=strlen(base);
      for(size_t i=0;i<L;i++){
        char c=base[i];
        guard[i]=isalpha(c)?toupper(c):'_';
      }
      memcpy(guard+L,"_PALETTE_H",10);
    }
    char fn_c[512];
    snprintf(fn_c,sizeof(fn_c),"ChasmPalette/Code_%s.h",base);
    { FILE*f=fopen(fn_c,"w");
      fprintf(f,
        "#ifndef %s\n#define %s\n\n#include <stdint.h>\n\n"
        "static const uint8_t %s_palette[256][3] = {\n",
        guard,guard,base);
      for(int i=0;i<256;i++){
        fprintf(f,"    { %3d, %3d, %3d }%s\n",
                pal_pink[i][0],pal_pink[i][1],pal_pink[i][2],
                i<255?",":"");
      }
      fprintf(f,"};\n\n#endif /* %s */\n",guard);
      fclose(f);
    }

    // 7) 3D LUT (.cube), mapping *original* palette (no pink override)
    const int N=16;
    // normalize original to [0,1]
    double palf_f[256][3];
    for(int i=0;i<256;i++){
      palf_f[i][0] = palf[i][0]/255.0;
      palf_f[i][1] = palf[i][1]/255.0;
      palf_f[i][2] = palf[i][2]/255.0;
    }
    char fn_cu[512];
    snprintf(fn_cu,sizeof(fn_cu),"ChasmPalette/Cube_%s.cube",base);
    { FILE*f=fopen(fn_cu,"w");
      if(!f) die("Failed to create .cube");
      fprintf(f,"TITLE \"%s\"\n",base);
      fprintf(f,"LUT_3D_SIZE %d\n",N);
      fprintf(f,"DOMAIN_MIN 0.0 0.0 0.0\n");
      fprintf(f,"DOMAIN_MAX 1.0 1.0 1.0\n");
      for(int bz=0;bz<N;bz++){
        double b = (double)bz/(N-1);
        for(int gy=0;gy<N;gy++){
          double g = (double)gy/(N-1);
          for(int rx=0;rx<N;rx++){
            double r = (double)rx/(N-1);
            // find nearest palette entry
            int best=0; double bestd=1e9;
            for(int i=0;i<256;i++){
              double dr = palf_f[i][0]-r;
              double dg = palf_f[i][1]-g;
              double db = palf_f[i][2]-b;
              double d = dr*dr+dg*dg+db*db;
              if(d<bestd){ bestd=d; best=i; }
            }
            fprintf(f,"%.6f %.6f %.6f\n",
                    palf_f[best][0],
                    palf_f[best][1],
                    palf_f[best][2]);
          }
        }
      }
      fclose(f);
    }

    printf("Wrote:\n"
           "  %s\n"
           "  %s\n"
           "  %s\n"
           "  %s\n"
           "  %s\n"
           "  %s\n"
           "  %s\n"
           "  %s\n"
           "%s\n",
           fn_a1,fn_a2,fn_q,fn_j,fn_p,fn_g,fn_c,fn_cu,
           mx<=63?"(6→8-bit scaled)":"");
    return 0;
}
