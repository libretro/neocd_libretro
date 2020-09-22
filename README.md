
# NeoCD-Libretro

## Introduction

NeoCD-Libretro is a complete rewrite of NeoCD from scratch in modern C++11. It is designed with accuracy and portability in mind rather than being all about speed like the the older versions. The goal is also to document all I know about the platform in the source code so other emulator authors can make their own implementations.

What is different?

* It's accurate. A lot more. As a result it uses more CPU power than older versions but requirements are still modest:  
  NeoCD runs perfect on Raspberry Pi 3 for example.
* It's implemented as a libretro core which allows it to be used everywhere, at home or on the go :)
* The CD-ROM drive is now emulated at hardware level.
* Supports savestates
* Supports rewind

## Credits

NeoCD would not have been possible without the following people generously sharing their code:

* Karl Stenerud - [Musashi 68000 Emulation Core](https://github.com/kstenerud/Musashi)
* Juergen Buchmueller - Z80 Emulation Core
* Jarek Burczynski & Tatsuyuki Satoh - YM2610 Emulation Core
* [The MAME Development Team](http://www.mamedev.org/) - MAME, an invaluable source of knowledge about arcade machines.
* Romain Tisserand - libchdr (https://github.com/rtissera/libchdr)

## Installation

### Core files

Copy the `libneocd_libretro` library to `RetroArch/cores`.

### The INFO file (cosmetic)

Copy `neocd_libretro.info` to folder `RetroArch/info`

### Required BIOS Files

To function NeoCD need a BIOS from a Front Loading, Top Loading or CDZ machine. The BIOS files should be installed in a `neocd` folder under RetroArch's system folder.

> **&#128211; Note:** The hashes are given to help you verify the files have not been tampered with, the emulator doesn't verify them.

#### Zoom ROM

> **&#128211; Note:** Need one in the following table

|Description        | Filename  | SHA1                                     |
|-------------------|-----------|------------------------------------------|
| Y Zoom ROM (Bad dump - MAME 0.127)       | ng-lo.rom | 2b1c719531dac9bb503f22644e6e4236b91e7cfc |
| Y Zoom ROM (MAME) | 000-lo.lo | 5992277debadeb64d1c1c64b0a92d9293eaf7e4a |

#### BIOS

> **&#128211; Note:** You need at least one in the following table. If several BIOSes are available, it will be possible to choose which to run in the Core Options Menu.
The files will be automatically byte swapped if needed. 

| Description                | Filename       | SHA1                                     |
|----------------------------|----------------|------------------------------------------|
| Front Loader BIOS          | neocd_f.rom    | a5f4a7a627b3083c979f6ebe1fabc5d2df6d083b |
| Front Loader BIOS (SMKDAN 0.7b DEC 2010) | neocd_sf.rom   | 4a94719ee5d0e3f2b981498f70efc1b8f1cef325 |
| Top Loader BIOS            | neocd_t.rom    | cc92b54a18a8bff6e595aabe8e5c360ba9e62eb5 |
| Top Loader BIOS (SMKDAN 0.7b DEC 2010) | neocd_st.rom   | 19729b51bdab60c42aafef6e20ea9234c7eb8410 |
| CDZ BIOS                   | neocd_z.rom    | b0f1c4fa8d4492a04431805f6537138b842b549f |
| CDZ BIOS (SMKDAN 0.7b DEC 2010) | neocd_sz.rom   | 6a947457031dd3a702a296862446d7485aa89dbb |
| Front Loader BIOS (MAME)   | front-sp1.bin  | 53bc1f283cdf00fa2efbb79f2e36d4c8038d743a |
| Top Loader BIOS (MAME)     | top-sp1.bin    | 235f4d1d74364415910f73c10ae5482d90b4274f |
| CDZ BIOS (MAME)            | neocd.bin      | 7bb26d1e5d1e930515219cb18bcde5b7b23e2eda |
| Universe BIOS CD 3.3       | uni-bioscd.rom | 5142f205912869b673a71480c5828b1eaed782a8 |

### CD Images

In the era of modern computers and portable devices, CD-ROMs are no longer convenient. Additionally I believe it is not possible to read the TOC of protected games without special drivers. As a result, NeoCD now exclusively run using CD-ROM images.

NeoCD accepts as input a cue sheet file (CUE) or a MAME CHD file. CUE images can be either of "single file" type (CUE, BIN) or "multiple files" type (CUE,ISO,[WAV/FLAC/OGG]).

> **&#127926; Supported audio formats are:** Wave (.wav), FLAC (.flac) or Ogg Vorbis (.ogg)

### The Core Options Menu

* **Region:** Change your Neo Geo CD's region. (changing this will reset the machine)
* **BIOS Select:** Select the BIOS to use here if you have several (changing this will reset the machine)
* **CD Speed Hack:** This will replace the BIOS CD-ROM busy loop with a STOP instruction, to help low powered platforms (like RPi) load faster (optional, changing this will reset the machine)
* **Skip CD Loading:** Settings this to ON makes the emulator auto fast forward CD loading sequences.

## For Developers

### Project Dependencies

* A C++11 compiler
* CMake > 3.0
* libFLAC
* libogg
* libvorbis
* zlib
* MSYS (Windows)

The project uses custom cmake finders in the folder `cmakescripts` to locate the libraries.

### Compiling

* Don't forget to --recurse-submodules when cloning
* Make sure the development packages for libFLAC, libogg and libvorbis are installed.
* Eventually, edit the CFLAGS in CMakeLists.txt to suit your platform (Raspberry Pi...)
* Invoke CMake: `cmake -G "Unix Makefiles" .` or `cmake -G "MSYS Makefiles" .` (Windows)
* If all went well, build: `make -j 4`
* Copy the resulting library in `RetroArch/cores`
* Done! (see the user section for the rest)

## Tested platforms

* x64 / Windows / GCC 9.1
* x64 / Arch Linux / GCC 9.1
* Raspberry Pi 3 / Arch Linux / GCC 8.2

## Known problems

* On Raspberry Pi set the CPU governor to "performance" to avoid possible micro stutter.
* In Metal Slug 2 sound glitches occasionally on Front or Top loader BIOSes, use the CDZ bios.
* In Galaxy Fight the raster effects glitch in demo mode but strangely work fine when playing.
