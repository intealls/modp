CC = gcc
CXX = g++
WARNINGS = -pedantic -Wall -Wextra -Wno-unused-function -Wno-overlength-strings
CFLAGS = -fno-omit-frame-pointer -O3 -march=native $(WARNINGS) -std=c11 -D_USE_MATH_DEFINES -D_DEFAULT_SOURCE
CXXFLAGS = -fno-omit-frame-pointer -O3 -march=native $(WARNINGS) -D_USE_MATH_DEFINES -D_DEFAULT_SOURCE
LIBS =  -L/usr/local/lib/ -lxmp -lsidplayfp -lportaudio -lgme -larchive -lSDL2_image -lSDL2
INCLUDE = -I/usr/local/include/sidplay -Ideps/tinydir -I3rdparty/hvl -I3rdparty/libsidplayfp -Ideps/tomlc99 -Isrc

ODIR = bin
NAME = $(ODIR)/modp
UI = glui

OPENMPT_SUPPORT = 0

ifeq ($(OPENMPT_SUPPORT), 1)
	LIBS +=  -lopenmpt
	CFLAGS += -DHAVE_OPENMPT
	CXXFLAGS += -DHAVE_OPENMPT
endif

ifeq ($(UI),glui)
	LIBS += -lm -lfftw3f
	ifeq ($(OS),Windows_NT)
		LIBS += -lopengl32 -lglu32
	else
		LIBS += -lGL -lGLU
	endif
endif

ifeq ($(UI),ncursesui)
	LIBS += -lncurses
endif

ifeq ($(OS),Windows_NT)
	CFLAGS += -mwindows
	LIBS += -lmingw32 -lSDL2main -lSDL2
else
	LIBS += -lSDL2
endif


.PHONY: directories

all: release

release: directories executable

debug: CFLAGS := -g3 -O0 $(WARNINGS) -fsanitize=address
debug: CXXFLAGS := -g3 -O0 $(WARNINGS) -fsanitize=address
debug: NAME := $(NAME)_dbg
debug: directories executable

directories:
	mkdir -p $(ODIR)

executable:
	$(CC) $(CFLAGS) $(INCLUDE) -c src/*.c $(UI)/*.c 3rdparty/hvl/hvl_replay.c deps/tomlc99/toml.c $(LIBS)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c 3rdparty/libsidplayfp/libsidplayfp_wrap.cpp $(LIBS)
	$(CXX) $(CXXFLAGS) $(INCLUDE) *.o -o $(NAME) $(LIBS)

clean:
	find . -name "*.o" -type f -delete
	rm -rf $(NAME) $(NAME)_dbg $(ODIR)
