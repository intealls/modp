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

#### Windows/Linux

Uses MSYS2 on Windows.

- Download libopenmpt from [here](https://lib.openmpt.org/libopenmpt/download), build and install.
- Download game-music-emu 0.6.2, from [here](https://bitbucket.org/mpyne/game-music-emu/downloads), apply `contrib/gme-0.6.2-playlist_patch.diff`, build and install (use -G "MSYS Makefiles" if on Windows). The patch improves playlist compatibility with music files from \*.joshw.info.
- Install prerequisites (libportaudio, libarchive, SDL2, fftw etc).
- run `./configure && make`
To skip autotools just do `cp Makefile.orig Makefile && make`
## Notes

You can find a bunch of interesting bitmap fonts to try out [here](https://github.com/Tecate/bitmap-fonts), not all of them work though.
