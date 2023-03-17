<p align="center"><img src="extra/banner2x.png" alt="3DSCII Logo" /></p>

# 3DSCII
Homebrew software for drawing ASCII, PETSCII and other types of tile-based art on the Nintendo 3DS console

>This software is in **Beta**!  
Keep backups turned on, save a lot!  
If you're experiencing bugs or crashes, please leave an (link)Issue with as much information as you can!
Thanks!


# Installation
Download (link)dist.zip and unarchive **its contents** to the **root** of SD card, so that you have a **3dscii** folder in the root directory

Download the latest (link)3dscii-3dsx or (link)3dscii.cia release, and install it your preferred way

# Using 3DSCII
May be confusing (yeah)  

Short version (for now):  
Long-touch Buttons to see Hints, L/R Bumper to Switch Screens, Left Stick to Scroll, DPad Left/Right to Undo/Redo, Start/Select to enter menu

# Export to PNG
You can use [this tool](https://rollinbarrel.github.io/3dscii-hxRenderer/) to export your .3dscii files to PNG  

Built-in export coming someday...

# Playscii compatibility
3DSCII shares the formats of Palettes and Charsets with Playscii

This repository's *playscii* folder contains *in_3dscii.py* and *out_3dscii.py* files, which you can put into the Playscii's *formats* folder to use its **Import** ~~and **Export** (not yet)~~ options with .3dscii files

# License and Credits
Made by [Rollin'Barrel](https://github.com/RollinBarrel)  
3DSCII is licensed under the (link)MIT License

Palettes and Charsets (as well as a lot of ideas :) thx JP) bundled with the software are taken from the [Playscii](http://vectorpoem.com/playscii/) [(MIT License)](https://heptapod.host/jp-lebreton/playscii/-/blob/7ea67bd9a00988002d4e6af005e97954e9b06f9d/license.txt)

[stb_image (Public Domain)](https://github.com/nothings/stb/blob/5736b15f7ea0ffb08dd38af21067c314d6a3aae9/stb_image.h)  
[ini.h (Public Domain)](https://github.com/mattiasgustavsson/libs/blob/91a72b86fa862f485a2369c9a18c037fed85360e/ini.h)

This software links to:  
[libctru](https://github.com/devkitPro/libctru) [(License)](https://github.com/devkitPro/libctru/blob/b18f04d88739283f6ffb55fe5ea77c73796cf61b/README.md#license)  
[citro3d](https://github.com/devkitPro/citro3d) [(Zlib License)](https://github.com/devkitPro/citro3d/blob/e8825650c6013440aaf077a77ce6605ba014732a/LICENSE)  
[citro2d](https://github.com/devkitPro/citro2d) [(Zlib License)](https://github.com/devkitPro/citro2d/blob/1d9b05a04d9f5de657f6958c6d26fe7ed4e6f2a8/LICENSE)  
[Tex3DS](https://github.com/devkitPro/tex3ds) [(GPL-3.0 License)](https://github.com/devkitPro/tex3ds/blob/37220c696e873812df41af62d1db0b20c66661fe/COPYING)  