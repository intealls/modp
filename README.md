# modp

Video game music player.

![fft](https://github.com/intealls/modp/blob/master/fft.gif "fft")
![scope](https://github.com/intealls/modp/blob/master/scope.gif "scope")

Currently supported backends are libopenmpt and game music emulator. Should play many files directly with playlist support (for song lengths and titles) from  \*.joshw.info.

Command line options:

### `modp-gl` (OpenGL UI)

| Option | Default | Description |
|---|---|---|
| `--path <dir>` | `.` | Initial music directory path |
| `--font <path>` | *(internal)* | Path to a BDF font file |
| `--cursor <path>` | *(default)* | Path to a custom cursor image |
| `--config <path>` | `~/.modp/modp.toml` | Configuration file path |
| `--createconfig` | — | Create a configuration file at the config path |
| `--showconfig` | — | Show the current resolved configuration |
| `--auto_increment <bool>` | `true` | Auto-advance at minimum song length or song end |
| `--random_auto_increment <bool>` | `false` | Pick a random song on auto-advance |
| `--song_min_length <n>` | `0` | Minimum song length before auto-advance (0–999 seconds) |
| `--width <n>` | `800` | Window width (320–1280) |
| `--height <n>` | `480` | Window height (240–960) |
| `--framelimit <n>` | `60` | Framerate limit (1–240) |
| `--bg_red <f>` | `0.00` | Background color red component (0.0–1.0) |
| `--bg_green <f>` | `0.33` | Background color green component (0.0–1.0) |
| `--bg_blue <f>` | `0.67` | Background color blue component (0.0–1.0) |
| `--fontstretch <bool>` | `false` | Pixel-double font vertically |
| `--font_shake_factor <f>` | `0.0` | Shake on-screen text in tune with music (0–9000) |
| `--font_zoom_factor <f>` | `0.0` | Zoom on-screen text in tune with music (0–9000) |
| `--font_rotation_factor <f>` | `0.0` | Rotate on-screen text in tune with music (0–9000) |
| `--bg_flash_factor <f>` | `0.0` | Flash background in tune with music (0–9000) |
| `--help` | — | Display help and exit |

### `modp-cli` (ncurses/terminal UI)

| Option | Default | Description |
|---|---|---|
| `--path <dir>` | `.` | Initial music directory path |
| `--config <path>` | `~/.modp/modp.toml` | Configuration file path |
| `--createconfig` | — | Create a configuration file at the config path |
| `--showconfig` | — | Show the current resolved configuration |
| `--auto_increment <bool>` | `true` | Auto-advance at minimum song length or song end |
| `--random_auto_increment <bool>` | `false` | Pick a random song on auto-advance |
| `--song_min_length <n>` | `0` | Minimum song length before auto-advance (0–999 seconds) |
| `--help` | — | Display help and exit |

All `--help` and `--createconfig` flags are recognized by both binaries. Options marked without a default are only valid on the command line and cannot be set in the configuration file.

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
