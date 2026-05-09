# modp

Video game music player.

![fft](https://github.com/intealls/modp/blob/master/fft.gif "fft")
![scope](https://github.com/intealls/modp/blob/master/scope.gif "scope")

Currently supported backends are libopenmpt and game music emulator. Should play many files directly with playlist support (for song lengths and titles) from  \*.joshw.info.

Command line options:

```
-p    Initial path, default is "."
-f    Path to a BDF font, default is an internal font
-v    Pixel-double font vertically, default is 0
-a    Auto increment at min length/song end, default is 1
-n    Random song at auto increment, default is 0
-m    Song minimum length, default is 0
-w    Window width, default is 800
-e    Window height, default is 480
-l    Framerate limit, default is 60.00
-r    Background color red component, default is 0.00
-g    Background color green component, default is 0.33
-b    Background color blue component, default is 0.67

-h    Show default command line options
```

## Building

### Dependencies

All platforms require the following libraries:

| Library | Purpose |
|---|---|
| SDL2 | Window/input |
| SDL2_image | Image loading (fonts) |
| OpenGL / GLU | Graphics (GL UI) |
| ncurses | Terminal UI (CLI) |
| fftw3 | FFT visualization |
| portaudio | Audio playback |
| libsidplayfp | Commodore 64 SID playback |
| libxmp | .mod/.s3m/.xm playback |
| libgme | Game music emulation |
| libopenmpt | Tracker module playback (**optional**) |
| libarchive | Archive reading |

#### Arch Linux

```bash
sudo pacman -S base-devel cmake sdl2 sdl2_image libglu portaudio fftw libsidplayfp libxmp libgme libarchive ncurses
# Optional: sudo pacman -S libopenmpt   (required if -DENABLE_OPENMPT=ON)
```

#### Ubuntu (22.04+)

```bash
sudo apt update
sudo apt install build-essential cmake libsdl2-dev libsdl2-image-dev libglu1-mesa-dev libfftw3-dev portaudio19-dev libsidplayfp-dev libxmp-dev libgme-dev libarchive-dev libncurses-dev
# Optional: sudo apt install libopenmpt-dev   (required if -DENABLE_OPENMPT=ON)
```

### CMake (recommended)

```bash
git clone https://github.com/intealls/modp.git
cd modp
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build .
```

Binaries are placed in `build/bin/` (`modp-gl`, `modp-cli`).

Options:

| Flag | Default | Description |
|---|---|---|
| `-DBUILD_GLUI=ON` | ON | Build the OpenGL UI |
| `-DBUILD_NCURSES=ON` | ON | Build the ncurses UI |
| `-DBUILD_DEBUG=OFF` | OFF | Debug build with sanitizers |
| `-DENABLE_OPENMPT=ON` | OFF | Enable libopenmpt tracker module backend |

### Makefile (legacy)

Uses MSYS2 on Windows.

- Download libopenmpt from [here](https://lib.openmpt.org/libopenmpt/download), build and install.
- Download game-music-emu 0.6.2, from [here](https://bitbucket.org/mpyne/game-music-emu/downloads), apply `contrib/gme-0.6.2-playlist_patch.diff`, build and install (use -G "MSYS Makefiles" if on Windows). The patch improves playlist compatibility with music files from \*.joshw.info.
- Install prerequisites (libportaudio, libarchive, SDL2, fftw etc).
- run `make`

#### Ubuntu with patched libgme

If you need the playlist patch for libgme:

```bash
git clone https://github.com/intealls/modp.git
cd modp
git clone https://bitbucket.org/mpyne/game-music-emu.git
cd game-music-emu/
# patch, build and install gme
patch -p1 < ../contrib/gme-b3d158a-playlist.patch
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
make && make install
cd ../..
# build modp
mkdir build && cd build
cmake ..
cmake --build .
```
## Notes

You can find a bunch of interesting bitmap fonts to try out [here](https://github.com/Tecate/bitmap-fonts), not all of them work though.
