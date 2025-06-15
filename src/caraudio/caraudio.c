// caraudio.c
// Extracts PCM audio from a Chasm .car file into either RAW or WAV files.
// Usage:
//   caraudio.exe <input.car> [prefix] [-raw|-wav]
//
// Examples:
//   caraudio.exe monster.car             → sound_0.raw … sound_6.raw
//   caraudio.exe monster.car fx -wav     → fx_0.wav … fx_6.wav
//
// Compile under WSL/MinGW:
//   x86_64-w64-mingw32-gcc -std=c11 -O2 -static \
//       -o caraudio.exe caraudio.c

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#pragma pack(push,1)
typedef struct {
    uint16_t animations[20];
    uint16_t submodels_animations[3][2];
    uint16_t unknown0[9];
    uint16_t sounds[7];       // lengths (bytes) of each PCM blob
    uint16_t unknown1[9];
} CARHeader;
#pragma pack(pop)

typedef enum { MODE_RAW, MODE_WAV } OutputMode;

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s <input.car> [prefix] [-raw|-wav]\n"
        "  <input.car>  Path to .car file\n"
        "  [prefix]     Output filename prefix (default: sound)\n"
        "  -raw         Emit .raw files (default)\n"
        "  -wav         Emit .wav files\n",
        prog
    );
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    const char *in_path = argv[1];
    const char *prefix = "sound";
    OutputMode mode = MODE_RAW;

    // parse remaining args
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "-raw") == 0) {
            mode = MODE_RAW;
        }
        else if (strcmp(argv[i], "-wav") == 0) {
            mode = MODE_WAV;
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown flag: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
        else {
            // first non-flag is prefix
            prefix = argv[i];
        }
    }

    FILE *f = fopen(in_path, "rb");
    if (!f) {
        perror("Error opening .car file");
        return 1;
    }

    // determine file size
    if (fseek(f, 0, SEEK_END) != 0) { perror("fseek"); fclose(f); return 1; }
    long file_size = ftell(f);
    if (file_size < 0) { perror("ftell"); fclose(f); return 1; }
    rewind(f);

    // read header
    CARHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fprintf(stderr, "Failed to read CAR header\n");
        fclose(f);
        return 1;
    }

    // sum sound lengths
    uint32_t total = 0;
    for (int i = 0; i < 7; ++i) total += hdr.sounds[i];

    // seek to audio data
    long audio_offset = file_size - (long)total;
    if (fseek(f, audio_offset, SEEK_SET) != 0) {
        perror("fseek to audio region");
        fclose(f);
        return 1;
    }

    // WAV parameters
    const uint32_t sampleRate    = 11025;
    const uint16_t numChannels   = 1;
    const uint16_t bitsPerSample = 8;
    const uint32_t byteRate      = sampleRate * numChannels * (bitsPerSample/8);
    const uint16_t blockAlign    = numChannels * (bitsPerSample/8);

    // extract each blob
    for (int i = 0; i < 7; ++i) {
        uint16_t len = hdr.sounds[i];
        if (len == 0) continue;

        char outname[256];
        if (mode == MODE_RAW)
            snprintf(outname, sizeof(outname), "%s_%d.raw", prefix, i);
        else
            snprintf(outname, sizeof(outname), "%s_%d.wav", prefix, i);

        FILE *out = fopen(outname, "wb");
        if (!out) {
            perror("Error creating output file");
            if (fseek(f, len, SEEK_CUR) != 0) perror("skip fseek");
            continue;
        }

        uint8_t *buf = malloc(len);
        if (!buf) {
            fprintf(stderr, "Memory allocation failed\n");
            fclose(out);
            if (fseek(f, len, SEEK_CUR) != 0) perror("skip fseek");
            continue;
        }
        if (fread(buf, 1, len, f) != len) {
            fprintf(stderr, "Read error at blob %d\n", i);
            free(buf);
            fclose(out);
            continue;
        }

        if (mode == MODE_WAV) {
            uint32_t dataSize  = len;
            uint32_t chunkSize = 36 + dataSize;
            // RIFF header
            fwrite("RIFF", 1, 4, out);
            fwrite(&chunkSize, 4, 1, out);
            fwrite("WAVE", 1, 4, out);
            // fmt subchunk
            fwrite("fmt ", 1, 4, out);
            uint32_t subSize = 16;
            uint16_t fmtPCM  = 1;
            fwrite(&subSize, 4, 1, out);
            fwrite(&fmtPCM, 2, 1, out);
            fwrite(&numChannels, 2, 1, out);
            fwrite(&sampleRate, 4, 1, out);
            fwrite(&byteRate, 4, 1, out);
            fwrite(&blockAlign, 2, 1, out);
            fwrite(&bitsPerSample, 2, 1, out);
            // data subchunk
            fwrite("data", 1, 4, out);
            fwrite(&dataSize, 4, 1, out);
        }

        // write PCM/raw data
        fwrite(buf, 1, len, out);

        // print result
        if (mode == MODE_WAV) {
            printf("Wrote %s (%u bytes of %u-bit @ %u Hz)\n",
                   outname,
                   (unsigned)len,
                   (unsigned)bitsPerSample,
                   (unsigned)sampleRate);
        } else {
            printf("Wrote %s (%u bytes)\n", outname, (unsigned)len);
        }

        free(buf);
        fclose(out);
    }

    fclose(f);
    return 0;
}
