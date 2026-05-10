CC = cc
CXX = c++
WARNINGS = -pedantic \
           -Wall \
           -Wextra \
           -Wformat=2 \
           -Wformat-security \
           -Wnull-dereference \
           -Wshadow \
           -Wwrite-strings \
           -Wvla \
           -Warray-bounds=2 \
           -Wno-unused-function \
           -Wno-overlength-strings
CFLAGS = -fno-omit-frame-pointer -O3 -march=native $(WARNINGS) -std=c11 -D_USE_MATH_DEFINES -D_DEFAULT_SOURCE
CXXFLAGS = -fno-omit-frame-pointer -O3 -march=native $(WARNINGS) -D_USE_MATH_DEFINES -D_DEFAULT_SOURCE

# Common libraries for all UIs
LIBS_COMMON = -L/usr/local/lib/ -lxmp -lsidplayfp -lportaudio -lgme -larchive -lSDL2_image

# glui specific libraries
LIBS_GLUI = -lm -lfftw3f

# ncursesui specific libraries
LIBS_NCURSES = -lncurses -lm -lfftw3f

# Platform-specific libraries (base)
ifeq ($(OS),Windows_NT)
	LIBS_PLATFORM_BASE = -lmingw32 -lSDL2main
	CFLAGS += -mwindows
else
	LIBS_PLATFORM_BASE = -lSDL2
endif

# glui platform-specific libraries
LIBS_PLATFORM_GLUI = -lGL -lGLU

# ncursesui platform-specific libraries (no OpenGL)
LIBS_PLATFORM_NCURSES =

INCLUDE = -I/usr/local/include/sidplay -Ideps/tinydir -I3rdparty/hvl -I3rdparty/libsidplayfp -Ideps/tomlc99 -Isrc

ODIR = bin
NAME_GLUI = $(ODIR)/modp-gl
NAME_NCURSES = $(ODIR)/modp-cli


.PHONY: directories all glui ncursesui

all: directories glui ncursesui

glui: directories $(NAME_GLUI)

ncursesui: directories $(NAME_NCURSES)

# Suppress warnings for third-party code
WARNINGS_THIRDPARTY = -w

# Build glui executable
$(NAME_GLUI): src/*.c glui/*.c 3rdparty/hvl/hvl_replay.c deps/tomlc99/toml.c 3rdparty/libsidplayfp/libsidplayfp_wrap.cpp
	$(CC) $(CFLAGS) $(INCLUDE) -c src/*.c glui/*.c deps/tomlc99/toml.c
	$(CC) $(CFLAGS) $(INCLUDE) $(WARNINGS_THIRDPARTY) -c 3rdparty/hvl/hvl_replay.c
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(WARNINGS_THIRDPARTY) -c 3rdparty/libsidplayfp/libsidplayfp_wrap.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) *.o -o $(NAME_GLUI) $(LIBS_COMMON) $(LIBS_GLUI) $(LIBS_PLATFORM_BASE) $(LIBS_PLATFORM_GLUI)
	rm -f *.o

# Build ncursesui executable
$(NAME_NCURSES): src/*.c ncursesui/*.c 3rdparty/hvl/hvl_replay.c deps/tomlc99/toml.c 3rdparty/libsidplayfp/libsidplayfp_wrap.cpp
	$(CC) $(CFLAGS) $(INCLUDE) -c src/*.c ncursesui/*.c deps/tomlc99/toml.c
	$(CC) $(CFLAGS) $(INCLUDE) $(WARNINGS_THIRDPARTY) -c 3rdparty/hvl/hvl_replay.c
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(WARNINGS_THIRDPARTY) -c 3rdparty/libsidplayfp/libsidplayfp_wrap.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) *.o -o $(NAME_NCURSES) $(LIBS_COMMON) $(LIBS_NCURSES) $(LIBS_PLATFORM_BASE) $(LIBS_PLATFORM_NCURSES)
	rm -f *.o

debug: CFLAGS := -g3 -O0 $(WARNINGS) -fsanitize=address,undefined -fno-omit-frame-pointer
debug: CXXFLAGS := -g3 -O0 $(WARNINGS) -fsanitize=address,undefined -fno-omit-frame-pointer
debug: NAME_GLUI := $(ODIR)/modp-gl_dbg
debug: NAME_NCURSES := $(ODIR)/modp-cli_dbg
debug: all

# Static analysis target (requires clang-tidy and cppcheck)
analyze:
	@echo "Running clang-tidy..."
	clang-tidy src/*.c -- $(INCLUDE) $(WARNINGS) -std=c11 2>&1 || true
	@echo "Running cppcheck..."
	cppcheck --enable=all --suppress=missingIncludeSystem src/*.c 2>&1 || true

directories:
	mkdir -p $(ODIR)

clean:
	find . -name "*.o" -type f -delete
	rm -rf $(NAME_GLUI) $(NAME_NCURSES) $(ODIR)/modp-gl_dbg $(ODIR)/modp-cli_dbg $(ODIR)
