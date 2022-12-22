AUTOMAKE_OPTIONS = foreign subdir-objects

if DEBUG
AM_CFLAGS = -Isrc -Iglui -Ideps/tinydir -I3rdparty/hvl -I3rdparty/libsidplayfp -g3 -O0 -fsanitize=address -Wall -Wextra -Wno-unused-function -Wno-overlength-strings
AM_CXXFLAGS = -Isrc -Iglui -Ideps/tinydir -I3rdparty/hvl -I3rdparty/libsidplayfp -g3 -O0 -fsanitize=address -Wall -Wextra -Wno-unused-function -Wno-overlength-strings
else
AM_CFLAGS = -Isrc -Iglui -Ideps/tinydir -I3rdparty/hvl -I3rdparty/libsidplayfp -fno-omit-frame-pointer -O2 -Wall -Wextra -Wno-unused-function -Wno-overlength-strings -std=c11 -D_USE_MATH_DEFINES -D_DEFAULT_SOURCE
AM_CXXFLAGS = -Isrc -Iglui -Ideps/tinydir -I3rdparty/hvl -I3rdparty/libsidplayfp -fno-omit-frame-pointer -O2 -pedantic -Wall -Wextra -Wno-unused-function -Wno-overlength-strings -D_USE_MATH_DEFINES -D_DEFAULT_SOURCE
endif

if WINDOWS
AM_CFLAGS += -mwindows
AM_CXXFLAGS += -mwindows
endif

bin_PROGRAMS = modp
modp_SOURCES = 3rdparty/hvl/hvl_replay.c 3rdparty/libsidplayfp/libsidplayfp_wrap.cpp src/AudioManager.c src/Player.c src/OpenMPTRenderer.c src/HVLRenderer.c src/HCS64File.c src/LocalDir.c src/GMERenderer.c src/XMPRenderer.c src/SIDRenderer.c glui/GL.c glui/Font.c glui/Main.c glui/GLWindow.c
modp_LDADD = -L/usr/local/lib/
