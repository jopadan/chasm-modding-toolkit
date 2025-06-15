// caraudio2.c
// Extracts PCM audio from a Chasm .car file into RAW or WAV files,
// and can import replacements to a new .car.
//
// Export mode:
//   caraudio2 -export <input.car> [prefix] [-raw|-wav]
//     <input.car>   Path to .car file
//     [prefix]      Output filename prefix (default: input filename without .car)
//     -raw          Emit .raw files (default)
//     -wav          Emit .wav files
//
// Import mode:
//   caraudio-io -import <input.car> <output.car> slot_n.raw [slot_m.raw ...]
//   caraudio-io -import <input.car> <output.car> slot_n.wav [slot_m.wav ...]
//     slot_n.<ext>  Replacement file ending in _n.raw or _n.wav (n = 0–6)
//                   size must match original slot
//
// Compile under WSL/MinGW:
//   sudo apt update && sudo apt install mingw-w64
//   x86_64-w64-mingw32-gcc -std=c11 -O2 -Wall -static -o caraudio-io.exe caraudio-io.c

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>  // strcasecmp()

#ifdef _WIN32
  #include <windows.h>
#endif

#pragma pack(push,1)
typedef struct {
    uint16_t animations[20];
    uint16_t submodels_animations[3][2];
    uint16_t unknown0[9];
    uint16_t sounds[7];
    uint16_t unknown1[9];
} CARHeader;
#pragma pack(pop)

typedef enum { MODE_EXPORT, MODE_IMPORT } ProgramMode;
typedef enum { OUT_RAW, OUT_WAV } ExportFormat;

typedef struct {
    int         slot;
    const char *path;
    int         is_wav;
} Replacement;

// Case-insensitive ends-with
static int endswith(const char *s, const char *suffix) {
    size_t sl = strlen(s), su = strlen(suffix);
    if (sl < su) return 0;
    return strcasecmp(s + sl - su, suffix) == 0;
}

// Load WAV PCM data (8-bit mono). Returns malloc'd buffer and sets out_len, or NULL.
static uint8_t *load_wav(const char *path, uint32_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }
    char riff[4], wave[4];
    uint32_t sz;
    if (fread(riff,1,4,f)!=4 || memcmp(riff,"RIFF",4)) { fclose(f); return NULL; }
    fread(&sz,4,1,f);
    fread(wave,1,4,f);
    if (memcmp(wave,"WAVE",4)) { fclose(f); return NULL; }

    uint16_t fmt=0,ch=0,bits=0;
    uint32_t rate=0,br=0;
    uint16_t align=0;
    uint8_t *buf=NULL;
    uint32_t dataSize=0;

    while (!feof(f)) {
        char id[4]; uint32_t chunk;
        if (fread(id,1,4,f)!=4) break;
        fread(&chunk,4,1,f);
        if (!memcmp(id,"fmt ",4)) {
            fread(&fmt,2,1,f);
            fread(&ch,2,1,f);
            fread(&rate,4,1,f);
            fread(&br,4,1,f);
            fread(&align,2,1,f);
            fread(&bits,2,1,f);
            if (chunk > 16) fseek(f, chunk - 16, SEEK_CUR);
            if (fmt!=1 || ch!=1 || bits!=8) { fclose(f); return NULL; }
        } else if (!memcmp(id,"data",4)) {
            dataSize = chunk;
            buf = malloc(dataSize);
            if (!buf) { fclose(f); return NULL; }
            if (fread(buf,1,dataSize,f)!=dataSize) { free(buf); buf = NULL; }
            break;
        } else {
            fseek(f, chunk, SEEK_CUR);
        }
    }
    fclose(f);
    if (buf) *out_len = dataSize;
    return buf;
}

// Load RAW PCM data. Returns malloc'd buffer and sets out_len, or NULL.
static uint8_t *load_raw(const char *path, uint32_t *out_len) {
    FILE *f = fopen(path,"rb");
    if (!f) { perror(path); return NULL; }
    fseek(f,0,SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz < 0) { fclose(f); return NULL; }
    uint8_t *buf = malloc(sz);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf,1,sz,f)!=(size_t)sz) { free(buf); buf = NULL; }
    fclose(f);
    if (buf) *out_len = sz;
    return buf;
}

static void usage(const char *p) {
    fprintf(stderr,
        "Export mode:\n"
        "  %s -export <input.car> [prefix] [-raw|-wav]\n"
        "    <input.car>   Path to .car file\n"
        "    [prefix]      Output filename prefix (default: input filename without .car)\n"
        "    -raw          Emit .raw files (default)\n"
        "    -wav          Emit .wav files\n\n"
        "Import mode:\n"
        "  %s -import <input.car> <output.car> slot_n.raw [slot_m.raw ...]\n"
        "  %s -import <input.car> <output.car> slot_n.wav [slot_m.wav ...]\n"
        "    slot_n.<ext>  Replacement file ending in _n.raw or _n.wav (n = 0–6)\n"
        "                  size must match original slot\n",
        p,p,p
    );
}

int main(int argc, char *argv[]) {
    #ifdef _WIN32
      SetConsoleOutputCP(CP_UTF8);
    #endif

    if (argc < 3) { usage(argv[0]); return 1; }

    ProgramMode mode;
    if      (strcmp(argv[1], "-export")==0) mode = MODE_EXPORT;
    else if (strcmp(argv[1], "-import")==0) mode = MODE_IMPORT;
    else { usage(argv[0]); return 1; }

    const char *in_path = argv[2];

    // Import mode needs at least: prog, -import, in, out, one slot
    if (mode == MODE_IMPORT && argc < 5) {
        usage(argv[0]);
        return 1;
    }

    // Read .car into memory
    FILE *f = fopen(in_path, "rb");
    if (!f) { perror(in_path); return 1; }
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    rewind(f);
    uint8_t *car = malloc(file_size);
    if (!car) { fclose(f); return 1; }
    if (fread(car,1,file_size,f) != (size_t)file_size) {
        free(car);
        fclose(f);
        return 1;
    }
    fclose(f);

    CARHeader *hdr = (CARHeader*)car;
    uint32_t total = 0;
    for (int i = 0; i < 7; ++i) total += hdr->sounds[i];
    long audio_off = file_size - total;

    if (mode == MODE_EXPORT) {
        // Parse export args
        char prefix_buf[260];
        int prefix_set = 0;
        ExportFormat fmt = OUT_RAW;
        for (int i = 3; i < argc; ++i) {
            if      (strcmp(argv[i], "-raw") == 0) fmt = OUT_RAW;
            else if (strcmp(argv[i], "-wav") == 0) fmt = OUT_WAV;
            else if (!prefix_set && argv[i][0] != '-') {
                strncpy(prefix_buf, argv[i], sizeof(prefix_buf)-1);
                prefix_buf[sizeof(prefix_buf)-1] = '\0';
                prefix_set = 1;
            }
        }
        if (!prefix_set) {
            // default prefix: input filename without extension
            const char *b = strrchr(in_path, '/');
            b = b ? b+1 : in_path;
            const char *b2 = strrchr(b, '\\');
            b = b2 ? b2+1 : b;
            strncpy(prefix_buf, b, sizeof(prefix_buf)-1);
            prefix_buf[sizeof(prefix_buf)-1] = '\0';
            char *dot = strrchr(prefix_buf, '.');
            if (dot) *dot = '\0';
        }

        // WAV header params
        const uint32_t rate = 11025;
        const uint16_t ch = 1, bits = 8;
        const uint32_t br = rate * ch * (bits/8);
        const uint16_t align = ch * (bits/8);

        uint32_t off = audio_off;
        for (int i = 0; i < 7; ++i) {
            uint16_t len = hdr->sounds[i];
            if (!len) continue;

            char outname[260];
            if (fmt == OUT_RAW)
                snprintf(outname, sizeof(outname), "%s_%d.raw", prefix_buf, i);
            else
                snprintf(outname, sizeof(outname), "%s_%d.wav", prefix_buf, i);

            FILE *o = fopen(outname, "wb");
            if (!o) { off += len; continue; }

            if (fmt == OUT_WAV) {
                uint32_t ds = len, ck = 36 + ds;
                fwrite("RIFF",1,4,o); fwrite(&ck,4,1,o);
                fwrite("WAVE",1,4,o); fwrite("fmt ",1,4,o);
                uint32_t sub = 16; uint16_t pcm = 1;
                fwrite(&sub,4,1,o); fwrite(&pcm,2,1,o);
                fwrite(&ch,2,1,o); fwrite(&rate,4,1,o);
                fwrite(&br,4,1,o); fwrite(&align,2,1,o);
                fwrite(&bits,2,1,o); fwrite("data",1,4,o);
                fwrite(&ds,4,1,o);
            }

            fwrite(car + off, 1, len, o);
            printf("Wrote %s (%u bytes%s)\n",
                outname, (unsigned)len,
                fmt==OUT_WAV ? " of 8-bit @ 11025 Hz" : "");
            fclose(o);
            off += len;
        }

    } else {
        // IMPORT mode
        const char *out_path = argv[3];
        Replacement reps[7];
        int rc = 0;
        for (int i = 4; i < argc; ++i) {
            if (endswith(argv[i], ".raw") || endswith(argv[i], ".wav")) {
                const char *p = strrchr(argv[i], '_');
                if (!p) continue;
                int slot = atoi(p+1);
                if (slot < 0 || slot > 6) continue;
                reps[rc++] = (Replacement){ slot, argv[i], endswith(argv[i], ".wav") };
            }
        }
        for (int r = 0; r < rc; ++r) {
            int s = reps[r].slot;
            uint16_t orig_len = hdr->sounds[s];
            uint32_t newlen = 0;
            uint8_t *buf = reps[r].is_wav
                         ? load_wav(reps[r].path, &newlen)
                         : load_raw(reps[r].path, &newlen);
            if (!buf) continue;
            if (newlen != orig_len) {
                fprintf(stderr,
                    "Warning: %s is %u bytes but slot %d expects %u; skipping\n",
                    reps[r].path, newlen, s, (unsigned)orig_len);
                free(buf);
                continue;
            }
            uint32_t off = audio_off;
            for (int j = 0; j < s; ++j) off += hdr->sounds[j];
            memcpy(car + off, buf, newlen);
            printf("Replaced slot %d with %s\n", s, reps[r].path);
            free(buf);
        }
        FILE *o = fopen(out_path, "wb");
        if (!o) { perror(out_path); free(car); return 1; }
        fwrite(car, 1, file_size, o);
        fclose(o);
        printf("Wrote updated car to %s\n", out_path);
    }

    free(car);
    return 0;
}
