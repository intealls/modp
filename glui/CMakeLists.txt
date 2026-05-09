# OpenGL UI target: modp-gl

set(GLUI_SOURCES
    Main.c
    Font.c
    GL.c
    GLWindow.c
)

add_executable(modp_gl ${GLUI_SOURCES})

target_include_directories(modp_gl PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/deps/tinydir
    ${FFTW3F_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
)

target_compile_definitions(modp_gl PRIVATE ${COMMON_DEFINES})

target_compile_options(modp_gl PRIVATE
    ${WARNINGS}
    $<$<NOT:$<BOOL:${BUILD_DEBUG}>>:${RELEASE_FLAGS}>
    $<$<BOOL:${BUILD_DEBUG}>:${DEBUG_FLAGS}>
)

target_link_libraries(modp_gl PRIVATE
    modp_core
    SDL2::SDL2
    OpenGL::GL
    OpenGL::GLU
    ${FFTW3F_LIBRARIES}
    ${FFTW3F_LDFLAGS}
    ${M_LIBRARY}
)

if(BUILD_DEBUG)
    target_link_options(modp_gl PRIVATE ${DEBUG_FLAGS})
endif()

# Windows-specific: link SDL2main, set subsystem
if(WIN32)
    target_link_libraries(modp_gl PRIVATE SDL2::SDL2main)
    set_target_properties(modp_gl PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# Set output name to match Makefile convention
set_target_properties(modp_gl PROPERTIES
    OUTPUT_NAME modp-gl
)
