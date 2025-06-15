--------------------------------------------------------------------------------
Chasm Modding Toolkit by SMR9000
--------------------------------------------------------------------------------
version 2.8.4 2025-June-08
--------------------------------------------------------------------------------
IMPORTANT: Always run this toolkit and keep files in its root folder to avoid GUI errors.
--------------------------------------------------------------------------------
Official discord server: https://discord.gg/yNuHEbEg
--------------------------------------------------------------------------------
NEW IN VERSION 1.1 Package:
- Direct PNG file support (8bit indexed and full color with transparency, no need for RAW file conversion)
- ACT palette file support (Photoshop format)
- Better transparency handling
- Improved error messages

NEW IN VERSION 2.0 Package:
- Added chasm .car carviewer
- Updated GUI interface
- Updated source code

NEW IN VERSION 2.1 Package:
- Fixed bug with GUI not loading CLI executables

NEW IN VERSION 2.2 Package:
- Added animation preview support in carviewer.exe
- Added interpolation to animation

NEW IN VERSION 2.3 Package:
- Fixed Overwrite bug where GUI would not replace existing file when confirmed

NEW IN VERSION 2.4 Package:
- Added car2png.exe - Extracts texture from CAR file and saves as PNG
- Added caraudio.exe - Extracts audio in RAW or WAV file and saves as multiple files
- Updated Carreplace-GUI.exe wit texture extract functions
- Added new GUI TOOL - CAR AUDIO.exe
- Updated carviewer.exe to play sounds

NEW IN VERSION 2.5 Package:
- Updated Carviewer 1.9.3
- New OBJviewer.exe and OBJtool.exe for OBJ sprite editing

NEW IN VERSION 2.6 Package:
- Added CELViewer.exe and CELtool.exe for CHASM/Autodesk CEL editing
- Added new GUI TOOL - CEL TOOLKIT.exe

NEW IN VERSION 2.7 Package:
- Added 3oviewer.exe - Viewer for Chasm 3o+ani files and animations 
- Added SPRviewer.exe - SPR Viewer with simple import/export function built in    

NEW IN VERSION 2.8 Package:
- Added Floortool.exe (FLOOR.XX viewer, and editor)
- Added Floorflag.exe (FLOOR.XX flags viewer, and editor)
- Added File type icons 

NEW IN VERSION 2.8.1 Package:
- Added pal2all.exe - Tool for converting Chasm2.pal to multiple palette formats
- Added rgbtool.exe - Tool reads a 256×N .RGB blend table and a 768-byte .PAL file, then for each of the N rows generates multiple palette exports
- Added mpv.exe - Multi Palette viewer 
- Added cubegen.exe - 3DLUT .cube generator
- Updated source code and added app icons

NEW IN VERSION 2.8.2 Package:
- Added ojmv.exe (OBJ JSON MODEL VIEWER) - Enhanced OBJ + JSON Model Viewer

NEW IN VERSION 2.8.3 Package:
- Added hdri2skybox.exe - HDRI (png/jpg) to Cubemap Skybox for Chasm Remastered
- Added HDRI (png/jpg) sample files
- Added Chasm Skybox and 360 panorama Sky Viewer

NEW IN VERSION 2.8.4 Package:
- Added paledit.exe - Palette Editor for Chasm2.pal palette file
- removed icons and png duplicates from source folder
--------------------------------------------------------------------------------
FILES INCLUDED:

GUI TOOLS:
1. GUI TOOL - CAR TEXTURE.exe version 1.4 - GUI wrapper for carreplace.exe and carviewer.exe 
2. GUI TOOL - CAR TEXTURE.exe version 1.0 - GUI wrapper for caraudio-io.exe
3. GUI TOOL - OBJ TOOLKIT.exe version 1.0 - GUI wrapper for objviewer.exe and objtool.exe

CLI TOOLS:
4. carreplace.exe - version 1.1.0 CLI executable for chaning CAR textures
5. carviewer.exe version 1.9.3 (usage through GUI or manually: carviewer.exe <model.car>)
6. car2png.exe version 1.0 (usage through GUI or manually: car2png.exe <model.car>)
7. caraudio-io.exe version 1.0 - tool for extracting and replacing CAR audio files
8. objviewer.exe version 1.0 - tool for vieweing OBJ sprite files (located in BMP folder DOS)
9. objtool.exe version 1.0 - tool for manipulation and creation of new OBJ sprite files
10. celtool.exe version 1.4 - tool for manipulation and creation of new CEL texture files
11. celviewer.exe version 1.0.0 - CHASM/Autodesk CEL
12. 3oviewer.exe version 1.0.0 - 3o/ani model/animation viewer
13. sprviewer.exe version 1.0.0 - spr file viewer + IO functions built in (For transparency, use #040404)
13. floortool.exe version 1.0.2 - FLOOR.XX viewer, and editor
14. floorflag.exe version 1.0.2 - FLOOR.XX flags viewer, and editor
15. pal2all.exe verison 1.0.1 - Tool for converting Chasm2.pal to multiple palette formats
16. rgbtool.exe version 1.0.0 - Tool reads a 256×N .RGB blend table and a 768-byte .PAL file, then for each of the N rows generates multiple palette exports
17. mpv.exe version 1.0.3 - Multi Palette viewer 
18. cubegen.exe version 1.0.0 - 3DLUT .cube generator for color grading for Photoshop
19. ojmv.exe version 1.0.0 -(OBJ JSON MODEL VIEWER) Enhanced OBJ + JSON Model Viewer,  
    -Used to preview animated JSON and static OBJ MODEL files
    -Animated JSON will be used for ANI creation in another tool
    -This is not used for Chasm sprite OBJ files!
    -Check test/example files 
20. hdri2skybox.exe version 1.0.0 - HDRI (png/jpg) to Cubemap Skybox for Chasm Remastered
21. skyviewer.exe version 1.0.1 - Chasm Skybox and 360 panorama Sky Viewer
22. paledit.exe version 1.0.1 - Palette Editor for CHASM2.pal palette file

OTHER:
23. Source code for all tools

24. chasmpalette.act - The Chasm palette in photoshop ACT format
25. chasmpalette.o - The Chasm palette in C code format

26. Original Chasm skin files (for reference)
27. New skin files by SMR9000 and Railed Robin
28. Misc example files
29. File type icons + App icons
30. Readme.txt - This file

--------------------------------------------------------------------------------
TIPS:
- The tool will automatically find the closest palette match for PNG colors
- #040404 in palette is used for transparent pixels (some models have a bug while displaying in the viewer)
- Keep original full-color textures for SNEG sourceport
- Use included Viewer to view CAR files: carreplace.exe <car_filename> - this version (1.7) have chasmpalette embedded
- For best results, use the provided ACT palette file
- If you are viewing original model with included viewer , close the viewer before pressing the "Replace texture" button
- All CLI tools have instructions in them if yxou run them through CMD
- caraudio-io.exe: New sound files must be same length as original, saved as 8-bit @ 11025 Hz WAV or PCM RAW

--------------------------------------------------------------------------------
CARREPLACE.EXE CLI USAGE:
1. With palette: carreplace.exe <car_filename> <png_filename> -palette <palette.act>
2. Custom output: carreplace.exe <car_filename> <png_filename> -palette <palette.act> -output <newfile.car>
3. Help: carreplace.exe -help

TEXTURE PREPARATION:
1. Create your texture with or without transparency, 8bit indexed or full color and save it as .PNG file 
2. Run the tool with: carreplace.exe model.car texture.png -palette chasmpalette.act [-output newfile.car] 
3. (-output is optional)

IMPORTANT NOTES:
1. The skin image must have the same width and height as the original texture!
2. Width is always 64px (CAR file limitation)!
3. For transparency, use #040404 (RGB 4,4,4) in your texture

--------------------------------------------------------------------------------
OBJVIEWER.EXE CLI USAGE:
objviewer.exe <sprite.obj> [fps] (fps is not necessary to use it will autoload at 12 FPS)
you can drag and drop *.obj on objviewer.exe
--------------------------------------------------------------------------------

OBJTOOL.EXE CLI USAGE:
objtool -export   <sprite.obj>
-this will extract all OBJ frames as PNG data

objtool -dummy    <w> <h> <origin> <frames> <palette_idx>
- this will create new blank.obj file with custom width, height, origin, and palettecolor (from chasmpalette index 0-255)

objtool -create   <manifest.txt> <new.obj>
- this will read your manifest file, and create new OBJ file
- manifest looks like this: folder\sprite_0.png,width,height,origin (Example: testflame\testflame_0.png,32,48,16)
- each manifest line is new sprite for loading (Use max 15 frames or DOS game might crash)
- Origin is calculated form bottom of sprite left so if the sprite is 64*128 and origin 32 it will be at 0x32 coordinates

objtool -manifest <folder>
- this will read your folder with PNG frames and create manifest filke used for creating new OBJ files

IMPORTANT NOTES:
1. Recommended width 64px MAX, Recommended height 128px MAX, Recommended no. of frames 15 MAX. (game might crash if over 15)
2. be sure to convert your sprite frames to chasm palette
3. For transparency, use #fC00C8 in your texture (last index on chasm palette)

------------------------------
CARREPLACE.EXE CLI USAGE:
1. Usage: car2png <input.car>
------------------------------
CARAUDIO-IO CLI USAGE:
Export mode:
caraudio-io -export <input.car> [prefix] [-raw|-wav]
<input.car>   Path to .car file
[prefix]      Output filename prefix (default: input filename without .car)
-raw          Emit .raw files (default)
-wav          Emit .wav files

Import mode:
caraudio-io -import <input.car> <output.car> slot_n.raw [slot_m.raw ...]
caraudio-io -import <input.car> <output.car> slot_n.wav [slot_m.wav ...]
slot_n.<ext>  Replacement file ending in _n.raw or _n.wav (n = 0–6)
size must match original slot
------------------------------
CARVIEWER CLI USAGE:
carviewer <model.car> , Controls are displayed inside app:
- F1: Toggle Overlay
- Space: Play/Pause
- 1-0: Select Anim
- +/-: Cycle Anim
- R: Toggle Spin
- ESC: Reset
- W/S: Zoom
- A/D: Rotate
- TAB: Wireframe
- F: Texture Filter
- Arrows: Pan
- PgUp/Dn: Change BG
- Mouse Drag: Rotate
- F5-F11: Play Sound
------------------------------
CELVIEWER.EXE CLI USAGE  
celviewer.exe <file.cel> [initial_zoom] 
- zoom parameter is optional
- you can drag and drop *.cel on celviewer.exe
- Celviewer is built using information from https://github.com/AnimatorPro/Animator-DOS code
- Celviewer can be used to view both CHASM and Autodesk Animator CEL files
- All controls are displayed inside app:
------------------------------
CELTOOL.EXE CLI USAGE  
Usage:
celtool.exe -export <file.cel>
celtool.exe -convert <file.png> [-diffusion|-pattern|-noise]
- Diffusion|-pattern|-noise is optional
- For best result prepare your PNG image in your favorite image editor
- For transparency, use #fC00C8 in your texture (last index on chasm palette)
--------------------------------------------------------------------------------
MPV.EXE
Usage:
mpv -help
mpv <palette_file>

Supported formats:
  .act, .lmp    768-byte binary
  .pal          JASC-PAL ASCII or raw 768-byte .PAL
  .gpl          GIMP ASCII
  .txt          grid or raw-hex
  .aco          Adobe Color Swatch v1
  .h            C header [256][3]
  .png    8bit preview
--------------------------------------------------------------------------------
HDRI2SKYBOX  
HDRI to Skybox Converter for Chasm: The Rift Remastered

Usage:
  hdri2skybox <input.jpg/png> [faceSize]

Example:
  hdri2skybox.exe glacier.png 1024
  This will generate a cubemap with six images, each 1024×1024 pixels.

Features:
- Supports HDRI images in PNG or JPG format only.
- Drag-and-drop support: simply drag your HDRI image onto the executable to auto-generate skybox images.
- Example: For a 2048×1024 image, it will generate 6 cubemap images of 512×512 pixels.
- Automatically creates filenames and output folders based on the input image name.
- Skyboxes are referenced in the RESOURCE.XX file as: #sky=Glacier.cel
  This will load: Glacier.cel.0.png through Glacier.cel.5.png

- HDRI Source Recommendation:
- Use https://www.manyworlds.run to generate HDRI images suitable for skybox conversion.
--------------------------------------------------------------------------------

The MIT License (Non-Commercial Variant)

Copyright (c) 2025 SMR9000

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the “Software”), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, and sublicense copies of
the Software, **except** that:

  • The rights granted herein **do not include** the right to use the Software
    for commercial purposes, to charge for copies of the Software or any
    derivative works, or to otherwise earn money from the Software in any way.

The above copyright notice and this permission notice, **including this
non-commercial restriction**, shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.


--------------------------------------------------------------------------------
Chasm Modding Toolkit is using:
FREEGLUT: Freeglut, the Free OpenGL Utility Toolkit
https://github.com/freeglut/freeglut

GLEW: OpenGL Extension Wrangler Library
https://glew.sourceforge.net

STB: single-file public domain (or MIT licensed) libraries for C/C++
https://github.com/nothings/stb

AUTOIT: AutoIt3 BASIC-like scripting language designed for automating the Windows GUI and general scripting.
https://www.autoitscript.com/site/
--------------------------------------------------------------------------------