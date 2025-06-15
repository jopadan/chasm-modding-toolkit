// This verison will stay opened when you run it without arguments
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEXTURE_OFFSET 0x486c
#define VERSION "1.1.0"

// Function to load ACT palette file
int load_act_palette(const char* filename, unsigned char palette[256][3]) {
    FILE* file = fopen(filename, "rb");
    if (!file) return 0;

    // ACT files can be 256 or 768 bytes (256 RGB triplets)
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size != 256 && size != 768) {
        fclose(file);
        return 0;
    }

    // Read palette data
    if (size == 256) {
        // Indexed ACT (1 byte per entry)
        for (int i = 0; i < 256; i++) {
            unsigned char index;
            if (fread(&index, 1, 1, file) != 1) break;
            // Create grayscale palette from indices
            palette[i][0] = palette[i][1] = palette[i][2] = index;
        }
    } else {
        // RGB ACT (3 bytes per entry)
        for (int i = 0; i < 256; i++) {
            if (fread(palette[i], 1, 3, file) != 3) break;
        }
    }

    fclose(file);
    return 256;
}

// Function to find closest color in palette
int find_color_in_palette(unsigned char palette[256][3], unsigned char r, unsigned char g, unsigned char b) {
    int closest = 0;
    int min_dist = 255*255*3;
    
    for (int p = 0; p < 256; p++) {
        int dr = r - palette[p][0];
        int dg = g - palette[p][1];
        int db = b - palette[p][2];
        int dist = dr*dr + dg*dg + db*db;
        
        if (dist < min_dist) {
            min_dist = dist;
            closest = p;
            if (min_dist == 0) break; // exact match
        }
    }
    return closest;
}

// Function to convert PNG to RAW with palette support
unsigned char* png_to_raw(const char* png_filename, const char* palette_filename, 
                         int* width, int* height, int* raw_size) {
    // Load image with alpha channel
    int channels;
    unsigned char *image = stbi_load(png_filename, width, height, &channels, STBI_rgb_alpha);
    if (!image) {
        printf("Error loading image %s\n", png_filename);
        return NULL;
    }
    channels = 4; // We forced RGBA loading

    // Load palette if provided
    unsigned char palette[256][3] = {0};
    int use_palette = 0;
    int transparent_color_index = 4; // Default index for #040404
    
    if (palette_filename) {
        int colors_loaded = load_act_palette(palette_filename, palette);
        if (colors_loaded > 0) {
            use_palette = 1;
            printf("Loaded ACT palette with %d colors\n", colors_loaded);
            
            // Find #040404 in palette (RGB 4,4,4)
            transparent_color_index = find_color_in_palette(palette, 4, 4, 4);
            printf("Using palette index %d for transparent pixels (RGB %d,%d,%d)\n",
                   transparent_color_index,
                   palette[transparent_color_index][0],
                   palette[transparent_color_index][1],
                   palette[transparent_color_index][2]);
        } else {
            printf("Warning: Could not load palette, using grayscale\n");
        }
    }

    // Allocate buffer for raw data
    *raw_size = (*width) * (*height);
    unsigned char *raw_data = (unsigned char*)malloc(*raw_size);
    if (!raw_data) {
        stbi_image_free(image);
        printf("Memory allocation error for raw data\n");
        return NULL;
    }

    // Process image
    for (int i = 0; i < *width * *height; i++) {
        unsigned char pixel;
        int alpha = channels == 4 ? image[i*channels + 3] : 255;
        
        if (alpha < 255) {
            // Transparent pixel - use #040404 from palette
            pixel = transparent_color_index;
        }
        else if (use_palette) {
            // Find closest palette color
            int r = image[i*channels];
            int g = channels > 1 ? image[i*channels+1] : r;
            int b = channels > 2 ? image[i*channels+2] : r;
            pixel = find_color_in_palette(palette, r, g, b);
        }
        else if (channels >= 3) {
            // Grayscale conversion
            pixel = (unsigned char)(0.299f * image[i*channels] + 
                                    0.587f * image[i*channels+1] + 
                                    0.114f * image[i*channels+2]);
        }
        else {
            pixel = image[i];
        }
        
        raw_data[i] = pixel;
    }

    stbi_image_free(image);
    return raw_data;
}

// Function to replace the texture data in CAR file
void replace_texture(const char *car_filename, unsigned char *raw_data, long raw_size, const char *output_filename) {
    // Open the car file (read mode)
    FILE *car_file = fopen(car_filename, "rb");
    if (car_file == NULL) {
        perror("Error opening car file");
        free(raw_data);
        exit(1);
    }

    // Allocate buffer for the entire car file content
    fseek(car_file, 0, SEEK_END);
    long car_size = ftell(car_file);
    fseek(car_file, 0, SEEK_SET);

    unsigned char *car_data = (unsigned char *)malloc(car_size);
    if (car_data == NULL) {
        perror("Memory allocation error for car data");
        free(raw_data);
        fclose(car_file);
        exit(1);
    }

    // Read the entire car file content into memory
    fread(car_data, 1, car_size, car_file);
    fclose(car_file);  // Close the car file after reading

    // Replace the texture data in the car data at the specified offset
    for (long i = 0; i < raw_size; i++) {
        car_data[TEXTURE_OFFSET + i] = raw_data[i];
    }

    // Open the output file to write the modified content
    FILE *output_file = fopen(output_filename, "wb");
    if (output_file == NULL) {
        perror("Error opening output file");
        free(raw_data);
        free(car_data);
        exit(1);
    }

    // Write the modified car data to the new file
    fwrite(car_data, 1, car_size, output_file);

    // Clean up
    fclose(output_file);
    free(raw_data);
    free(car_data);
}

void show_help() {
    printf("\n");
    printf("Chasm CAR Model Texture replacement tool by SMR9000\n");
    printf("Version %s\n", VERSION);
    printf("\n");
    printf("Usage: carreplace <car_filename> <png_filename> [options]\n");
    printf("This program converts a PNG to indexed RAW format and replaces the texture in a Chasm .car model.\n\n");
    printf("Options:\n");
    printf("  -palette <file.act>  Use specified ACT palette file for conversion\n");
    printf("  -output <file.car>   Specify output filename (default: output.car)\n");
    printf("  -help                Display this help message\n");
    printf("\n");
    printf("TIPS:\n");
    printf("  - PNG will be converted to indexed RAW format using the specified palette\n");
    printf("  - #040404 in palette is used for transparency\n");
    printf("  - New texture must be the same dimensions as original\n");
    printf("\n");
    
    // Add pause for Windows
    #ifdef _WIN32
    system("pause");
    #endif
}

int main(int argc, char *argv[]) {
    // Show help if no arguments provided
    if (argc == 1) {
        show_help();
        return 0;
    }

    const char *car_filename = NULL;
    const char *png_filename = NULL;
    const char *palette_filename = NULL;
    const char *output_filename = "output.car";

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "/?") == 0) {
            show_help();
            return 0;
        }
        else if (strcmp(argv[i], "-palette") == 0) {
            if (i+1 < argc) {
                palette_filename = argv[++i];
            } else {
                printf("Error: -palette option requires a filename\n");
                show_help();
                return 1;
            }
        }
        else if (strcmp(argv[i], "-output") == 0) {
            if (i+1 < argc) {
                output_filename = argv[++i];
            } else {
                printf("Error: -output option requires a filename\n");
                show_help();
                return 1;
            }
        }
        else if (!car_filename) {
            car_filename = argv[i];
        }
        else if (!png_filename) {
            png_filename = argv[i];
        }
        else {
            printf("Error: Too many arguments\n");
            show_help();
            return 1;
        }
    }

    // Validate required arguments if any arguments were provided
    if (argc > 1 && (!car_filename || !png_filename)) {
        printf("Error: Missing required arguments\n");
        show_help();
        return 1;
    }

    // If we have both required arguments, proceed with conversion
    if (car_filename && png_filename) {
        // Convert PNG to RAW
        int width, height, raw_size;
        unsigned char *raw_data = png_to_raw(png_filename, palette_filename, &width, &height, &raw_size);
        if (!raw_data) {
            return 1;
        }

        printf("Converted %s to RAW (%dx%d)\n", png_filename, width, height);

        // Replace texture in CAR file
        replace_texture(car_filename, raw_data, raw_size, output_filename);
        printf("Texture replaced successfully. Saved as %s\n", output_filename);
    }

    return 0;
}