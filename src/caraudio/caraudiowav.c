
// Compile with: x86_64-w64-mingw32-gcc -std=c11 -O2 -Wall -o caraudiowav.exe caraudiowav.c

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#pragma pack(push,1)
typedef struct {
    uint16_t animations[20];
    uint16_t submodels_animations[3][2];
    uint16_t unknown0[9];
    uint16_t sounds[7];       // lengths (bytes) of each PCM blob
    uint16_t unknown1[9];
} CARHeader;
#pragma pack(pop)

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input.car> [output_prefix]\n", argv[0]);
        return 1;
    }
    const char *in_path = argv[1];
    const char *prefix  = (argc >= 3 ? argv[2] : "audio");

    FILE *f = fopen(in_path, "rb");
    if (!f) {
        perror("Error opening .car file");
        return 1;
    }

    // Determine file size
    if (fseek(f, 0, SEEK_END) != 0) { perror("fseek"); fclose(f); return 1; }
    long file_size = ftell(f);
    if (file_size < 0)           { perror("ftell"); fclose(f); return 1; }
    rewind(f);

    // Read the 0x66‐byte header
    CARHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fprintf(stderr, "Failed to read CAR header\n");
        fclose(f);
        return 1;
    }

    // Sum up all sound lengths
    uint32_t total_bytes = 0;
    for (int i = 0; i < 7; ++i) {
        total_bytes += hdr.sounds[i];
    }

    // Seek to start of audio region
    long audio_offset = file_size - (long)total_bytes;
    if (fseek(f, audio_offset, SEEK_SET) != 0) {
        perror("fseek to audio region");
        fclose(f);
        return 1;
    }

    // Extract each sound blob and write a WAV file
    for (int i = 0; i < 7; ++i) {
        uint16_t len = hdr.sounds[i];
        if (len == 0) continue;  // skip empty entries

        // Build output filename: e.g. "audio_0.wav"
        char outname[256];
        snprintf(outname, sizeof(outname), "%s_%d.wav", prefix, i);

        FILE *out = fopen(outname, "wb");
        if (!out) {
            perror("fopen output");
            // Skip ahead in input
            if (fseek(f, len, SEEK_CUR) != 0) perror("fseek skip");
            continue;
        }

        // Read PCM data into buffer
        uint8_t *buf = malloc(len);
        if (!buf) {
            fprintf(stderr, "malloc failed for %s\n", outname);
            fclose(out);
            if (fseek(f, len, SEEK_CUR) != 0) perror("fseek skip");
            continue;
        }
        if (fread(buf, 1, len, f) != len) {
            fprintf(stderr, "Read error for %s\n", outname);
            free(buf);
            fclose(out);
            continue;
        }

        // --- Write 44-byte WAV header ---
        uint32_t dataSize   = len;
        uint32_t chunkSize  = 36 + dataSize;
        uint32_t sampleRate = 11025;
        uint16_t numChannels   = 1;
        uint16_t bitsPerSample = 8;
        uint32_t byteRate    = sampleRate * numChannels * (bitsPerSample/8);
        uint16_t blockAlign  = numChannels * (bitsPerSample/8);

        // RIFF chunk
        fwrite("RIFF", 1, 4, out);
        fwrite(&chunkSize,    4, 1, out);
        fwrite("WAVE", 1, 4, out);

        // fmt subchunk
        fwrite("fmt ", 1, 4, out);
        uint32_t subChunk1Size = 16;      // PCM
        uint16_t audioFormat   = 1;       // PCM
        fwrite(&subChunk1Size, 4, 1, out);
        fwrite(&audioFormat,   2, 1, out);
        fwrite(&numChannels,   2, 1, out);
        fwrite(&sampleRate,    4, 1, out);
        fwrite(&byteRate,      4, 1, out);
        fwrite(&blockAlign,    2, 1, out);
        fwrite(&bitsPerSample, 2, 1, out);

        // data subchunk
        fwrite("data", 1, 4, out);
        fwrite(&dataSize, 4, 1, out);

        // PCM samples
        fwrite(buf, 1, len, out);

        printf("Wrote %s (%u bytes of 8-bit @ %u Hz)\n",
               outname, (unsigned)len, sampleRate);

        free(buf);
        fclose(out);
    }

    fclose(f);
    return 0;
}
