# Chasm Modding Toolkit by SMR9000

Chasm Modding Toolkit Package is a comprehensive Windows package for customizing and modding game assets for both DOS and REMAKE version of Chasm the Rift. It supports CAR, OBJ, CEL and SPR formats for models, sprites, textures, animations and many more. You can extract, replace and preview textures, edit palettes, import/export audio, generate skyboxes and cube maps, and view assets with dedicated GUI or CLI tools.

## Contents

- 3oviewer
- carreplace
- carviewer
- car2png
- caraudio
- carreplace
- carviewer
- celgui
- celtool
- celviewer
- cubegen
- floorflag
- floortool
- hdri2skybox
- mpv
- objtool
- objviewer
- ojmv
- pal2all
- paledit
- skyviewer
- sprviewer

## Usage

> [!TIP]
> - The tool will automatically find the closest palette match for PNG colors
> - #040404 in palette is used for transparent pixels (some models have a bug while displaying in the viewer)
> - Keep original full-color textures for SNEG sourceport
> - Use included Viewer to view CAR files: carreplace.exe <car_filename> - this version (1.7) have chasmpalette embedded
> - For best results, use the provided ACT palette file
> - If you are viewing original model with included viewer , close the viewer before pressing the "Replace texture" button
> - All CLI tools have instructions in them if yxou run them through CMD
> - caraudio-io.exe: New sound files must be same length as original, saved as 8-bit @ 11025 Hz WAV or PCM RAW

> [!IMPORTANT]
> - The skin image must have the same width and height as the original texture!
> - Width is always 64px (CAR file limitation)!
> - For transparency, use #040404 (RGB 4,4,4) in your texture
> - Recommended width 64px MAX, Recommended height 128px MAX, Recommended no. of frames 15 MAX. (game might crash if over 15)
> - be sure to convert your sprite frames to chasm palette
> - For transparency, use #fC00C8 in your texture (last index on chasm palette)

## Links

- [Chasm Modding Toolkit Package file - Chasm: The Rift - ModDB][1]
- [The Shadow Zone Discord Server][2]
- [AwesomeChasm][3]
- [freeglut][4]
- [GLEW][5]
- [stb][6]
- [autoit][7]

[1]: https://www.moddb.com/games/chasm-the-rift/downloads/chasm-modding-toolkit-package
[2]: https://discord.gg/f85Cz4FaXP
[3]: https://github.com/jopadan/AwesomeChasm
[4]: https://github.com/freeglut/freeglut
[5]: https://glew.sourceforge.net
[6]: https://github.com/nothings/stb
[7]: https://www.autoitscript.com/site/

