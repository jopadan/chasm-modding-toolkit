// Compile with: x86_64-w64-mingw32-gcc -std=c11 -O2 -static -o caraudioraw.exe caraudioraw.c

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#pragma pack(push,1)
typedef struct {
    uint16_t animations[20];
    uint16_t submodels_animations[3][2];
    uint16_t unknown0[9];
    uint16_t sounds[7];     // lengths of each PCM blob
    uint16_t unknown1[9];
} CARHeader;
#pragma pack(pop)

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input.car> [output_prefix]\n", argv[0]);
        return 1;
    }
    const char *in_path = argv[1];
    const char *prefix  = (argc >= 3 ? argv[2] : "sound");

    FILE *f = fopen(in_path, "rb");
    if (!f) { perror("fopen"); return 1; }

    // determine file size
    if (fseek(f, 0, SEEK_END) != 0) { perror("fseek"); fclose(f); return 1; }
    long file_size = ftell(f);
    if (file_size < 0) { perror("ftell"); fclose(f); return 1; }
    rewind(f);

    // read the 0x66-byte CARHeader at file start
    CARHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fprintf(stderr, "Failed to read CAR header\n");
        fclose(f);
        return 1;
    }

    // sum up all sound-blob lengths
    uint32_t total = 0;
    for (int i = 0; i < 7; ++i) {
        total += hdr.sounds[i];
    }

    // audio region is the last `total` bytes of the file
    long audio_offset = file_size - total;
    if (fseek(f, audio_offset, SEEK_SET) != 0) {
        perror("fseek to audio region");
        fclose(f);
        return 1;
    }

    // extract each blob into sound_0.raw … sound_6.raw
    for (int i = 0; i < 7; ++i) {
        uint16_t len = hdr.sounds[i];
        if (len == 0) continue;  // skip empty slots

        char outname[256];
        snprintf(outname, sizeof(outname), "%s_%d.raw", prefix, i);

        FILE *out = fopen(outname, "wb");
        if (!out) {
            perror("fopen output");
            // still advance the file pointer
            if (fseek(f, len, SEEK_CUR) != 0) perror("fseek skip");
            continue;
        }

        uint8_t *buf = malloc(len);
        if (!buf) {
            fprintf(stderr, "malloc failed for %s\n", outname);
            fclose(out);
            if (fseek(f, len, SEEK_CUR) != 0) perror("fseek skip");
            continue;
        }

        if (fread(buf, 1, len, f) != len) {
            fprintf(stderr, "read error for %s\n", outname);
        } else {
            fwrite(buf, 1, len, out);
            printf("Wrote %s (%u bytes)\n", outname, (unsigned)len);
        }

        free(buf);
        fclose(out);
    }

    fclose(f);
    return 0;
}
