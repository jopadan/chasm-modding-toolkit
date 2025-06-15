--------------------------------------------------------------------------------
Chasm Modding Toolkit by SMR9000
--------------------------------------------------------------------------------
version 2.8.4 2025-June-08
--------------------------------------------------------------------------------
Dependencies
--------------------------------------------------------------------------------
- [freeglut](https://github.com/freeglut/freeglut)
- [GLEW](https://glew.sourceforge.net)
- [stb](https://github.com/nothings/stb)
- [autoit](https://www.autoitscript.com/site/)
--------------------------------------------------------------------------------
23. Source code for all tools

24. chasmpalette.act - The Chasm palette in photoshop ACT format
25. chasmpalette.o - The Chasm palette in C code format

26. Original Chasm skin files (for reference)
27. New skin files by SMR9000 and Railed Robin
28. Misc example files
29. File type icons + App icons
30. README.md - This file
--------------------------------------------------------------------------------
Usage
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
