# Chasm Modding Toolkit by SMR9000

## Dependencies

- [freeglut](https://github.com/freeglut/freeglut)
- [GLEW](https://glew.sourceforge.net)
- [stb](https://github.com/nothings/stb)
- [autoit](https://www.autoitscript.com/site/)

## Contents

- carreplace
- carviewer

## Usage

TIPS:
- The tool will automatically find the closest palette match for PNG colors
- #040404 in palette is used for transparent pixels (some models have a bug while displaying in the viewer)
- Keep original full-color textures for SNEG sourceport
- Use included Viewer to view CAR files: carreplace.exe <car_filename> - this version (1.7) have chasmpalette embedded
- For best results, use the provided ACT palette file
- If you are viewing original model with included viewer , close the viewer before pressing the "Replace texture" button
- All CLI tools have instructions in them if yxou run them through CMD
- caraudio-io.exe: New sound files must be same length as original, saved as 8-bit @ 11025 Hz WAV or PCM RAW

TEXTURE PREPARATION:
1. Create your texture with or without transparency, 8bit indexed or full color and save it as .PNG file 
2. Run the tool with: carreplace.exe model.car texture.png -palette chasmpalette.act [-output newfile.car] 
3. (-output is optional)

IMPORTANT NOTES:
1. The skin image must have the same width and height as the original texture!
2. Width is always 64px (CAR file limitation)!
3. For transparency, use #040404 (RGB 4,4,4) in your texture

IMPORTANT NOTES:
1. Recommended width 64px MAX, Recommended height 128px MAX, Recommended no. of frames 15 MAX. (game might crash if over 15)
2. be sure to convert your sprite frames to chasm palette
3. For transparency, use #fC00C8 in your texture (last index on chasm palette)
