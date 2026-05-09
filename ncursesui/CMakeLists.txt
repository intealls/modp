# ncurses UI target: modp-cli

set(NCURSES_SOURCES
    Main.c
    NcursesWindow.c
)

add_executable(modp_cli ${NCURSES_SOURCES})

target_include_directories(modp_cli PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/deps/tinydir
    ${CURSES_INCLUDE_DIR}
    ${FFTW3F_INCLUDE_DIRS}
)

target_compile_definitions(modp_cli PRIVATE
    ${COMMON_DEFINES}
    ${CURSES_C_FLAGS}
)

target_compile_options(modp_cli PRIVATE
    ${WARNINGS}
    $<$<NOT:$<BOOL:${BUILD_DEBUG}>>:${RELEASE_FLAGS}>
    $<$<BOOL:${BUILD_DEBUG}>:${DEBUG_FLAGS}>
)

target_link_libraries(modp_cli PRIVATE
    modp_core
    ${CURSES_LIBRARY}
    ${FFTW3F_LIBRARIES}
    ${FFTW3F_LDFLAGS}
    ${M_LIBRARY}
)

if(BUILD_DEBUG)
    target_link_options(modp_cli PRIVATE ${DEBUG_FLAGS})
endif()

# Set output name to match Makefile convention
set_target_properties(modp_cli PROPERTIES
    OUTPUT_NAME modp-cli
)
