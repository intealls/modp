# Copyright intealls
# License: GPL v3

cmake_minimum_required(VERSION 3.14)
project(modp LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Options ---
option(BUILD_GLUI "Build the OpenGL (GL) UI variant" ON)
option(BUILD_NCURSES "Build the ncurses (CLI) UI variant" ON)
option(BUILD_DEBUG "Enable debug flags and sanitizers" OFF)
option(ENABLE_OPENMPT "Enable libopenmpt backend for tracker modules" OFF)

# --- Output dirs ---
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Generate compile_commands.json for IDE/LSP support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Compiler warnings (mirrors Makefile WARNINGS) ---
# All of these are GCC/Clang flags; MSVC gets different treatment
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(WARNINGS
        -pedantic
        -Wall
        -Wextra
        -Wformat=2
        -Wformat-security
        -Wnull-dereference
        -Wshadow
        -Wwrite-strings
        -Wvla
        -Warray-bounds=2
        -Wno-unused-function
        -Wno-overlength-strings
    )
else()
    set(WARNINGS)
endif()

# --- Debug / release flags ---
if(BUILD_DEBUG)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
        set(DEBUG_FLAGS
            -g3 -O0
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
    else()
        set(DEBUG_FLAGS -g3 -O0)
    endif()
else()
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
        set(RELEASE_FLAGS
            -fno-omit-frame-pointer
            -O3
            -march=native
            -UNDEBUG
        )
    else()
        set(RELEASE_FLAGS -O3 -UNDEBUG)
    endif()
endif()

# --- Common defines ---
set(COMMON_DEFINES
    _USE_MATH_DEFINES
)
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    list(APPEND COMMON_DEFINES _DEFAULT_SOURCE)
endif()

# ============================================================================
# Find external dependencies
#
# Strategy:
#   - pkg_check_modules()  for libs that ship .pc files (preferred)
#   - find_package()       for libs with CMake config/imported targets
#   - find_library()       fallback only when neither is available
# ============================================================================

find_package(PkgConfig REQUIRED)

# --- Libraries with .pc files ---
pkg_check_modules(FFTW3F     REQUIRED fftw3f)
pkg_check_modules(PORTAUDIO  REQUIRED portaudio-2.0)
pkg_check_modules(SIDPLAYFP  REQUIRED libsidplayfp)
pkg_check_modules(ARCHIVE    REQUIRED libarchive)
pkg_check_modules(SDL2IMAGE  REQUIRED SDL2_image)
pkg_check_modules(XMP        REQUIRED libxmp)
pkg_check_modules(GME        REQUIRED libgme)

if(ENABLE_OPENMPT)
    pkg_check_modules(OPENMPT  REQUIRED libopenmpt)
endif()

# --- Libraries with CMake config / good find modules ---
find_package(SDL2 REQUIRED)
if(BUILD_GLUI)
    find_package(OpenGL REQUIRED)
endif()
if(BUILD_NCURSES)
    find_package(Curses REQUIRED)
endif()

# --- Math ---
find_library(M_LIBRARY m REQUIRED)

# ============================================================================
# Shared static library: modp_core
# ============================================================================

set(MODP_CORE_SOURCES
    src/AudioManager.c
    src/GMERenderer.c
    src/HCS64File.c
    src/HVLRenderer.c
    src/LocalDir.c
    src/OpenMPTRenderer.c
    src/Option.c
    src/Player.c
    src/SIDRenderer.c
    src/Utils.c
    src/XMPRenderer.c
    3rdparty/hvl/hvl_replay.c
    deps/tomlc99/toml.c
    3rdparty/libsidplayfp/libsidplayfp_wrap.cpp
)

add_library(modp_core STATIC ${MODP_CORE_SOURCES})

target_include_directories(modp_core PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/deps/tinydir
    ${CMAKE_SOURCE_DIR}/deps/tomlc99
    ${CMAKE_SOURCE_DIR}/3rdparty/hvl
    ${CMAKE_SOURCE_DIR}/3rdparty/libsidplayfp
    ${SIDPLAYFP_INCLUDE_DIRS}
    ${SDL2IMAGE_INCLUDE_DIRS}
    ${PORTAUDIO_INCLUDE_DIRS}
    ${ARCHIVE_INCLUDE_DIRS}
    ${XMP_INCLUDE_DIRS}
    ${GME_INCLUDE_DIRS}
)

target_compile_definitions(modp_core PRIVATE ${COMMON_DEFINES})

if(ENABLE_OPENMPT)
    target_compile_definitions(modp_core PRIVATE HAVE_OPENMPT)
endif()

target_compile_options(modp_core PRIVATE
    ${WARNINGS}
    $<$<NOT:$<BOOL:${BUILD_DEBUG}>>:${RELEASE_FLAGS}>
    $<$<BOOL:${BUILD_DEBUG}>:${DEBUG_FLAGS}>
)

if(BUILD_DEBUG)
    target_link_options(modp_core PRIVATE ${DEBUG_FLAGS})
endif()

target_link_libraries(modp_core PRIVATE
    ${XMP_LIBRARIES}
    ${XMP_LDFLAGS}
    ${SIDPLAYFP_LIBRARIES}
    ${SIDPLAYFP_LDFLAGS}
    ${PORTAUDIO_LIBRARIES}
    ${PORTAUDIO_LDFLAGS}
    ${GME_LIBRARIES}
    ${GME_LDFLAGS}
    ${ARCHIVE_LIBRARIES}
    ${ARCHIVE_LDFLAGS}
    ${SDL2IMAGE_LIBRARIES}
    ${SDL2IMAGE_LDFLAGS}
)

if(ENABLE_OPENMPT)
    target_include_directories(modp_core PRIVATE ${OPENMPT_INCLUDE_DIRS})
    target_link_libraries(modp_core PRIVATE ${OPENMPT_LIBRARIES} ${OPENMPT_LDFLAGS})
endif()

# ============================================================================
# Add UI subdirectories
# ============================================================================

if(BUILD_GLUI)
    add_subdirectory(glui)
endif()

if(BUILD_NCURSES)
    add_subdirectory(ncursesui)
endif()

# ============================================================================
# Install rules
# ============================================================================

if(BUILD_GLUI)
    install(TARGETS modp_gl RUNTIME DESTINATION bin)
endif()
if(BUILD_NCURSES)
    install(TARGETS modp_cli RUNTIME DESTINATION bin)
endif()
