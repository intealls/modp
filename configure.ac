#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT([modp], [0.2], [], [], [https://github.com/intealls/modp/])
AM_INIT_AUTOMAKE
AC_CONFIG_FILES([Makefile])
AC_USE_SYSTEM_EXTENSIONS

# AC_CONFIG_HEADERS([config.h])
AC_CANONICAL_HOST
# Checks for programs.
AC_PROG_AWK
AC_PROG_CXX
AC_PROG_CC

AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug],
                  [whether to include debug symbols (default is no)])],
  [enable_debug=$enableval],
  [enable_debug=no]
)

AM_CONDITIONAL([DEBUG], [test x$enable_debug = xyes])
AM_CONDITIONAL([WINDOWS], [test x$windows = xtrue])
# Checks for libraries.
AC_CHECK_LIB([SDL2], [SDL_Init], [], [AC_MSG_ERROR([SDL2 is required])])
AC_CHECK_LIB([archive], [main], [], [AC_MSG_ERROR([archive lib is required])])
AC_CHECK_LIB([fftw3f], [main], [], [AC_MSG_ERROR([fftw3f is required])])
AC_CHECK_LIB([gme], [main], [], [AC_MSG_ERROR([gme lib is required])])
AC_CHECK_LIB([m], [main], [], [AC_MSG_ERROR([libm is required])])
AC_CHECK_LIB([openmpt], [main], [], [AC_MSG_ERROR([openmpt is required])])
AC_CHECK_LIB([portaudio], [main], [], [AC_MSG_ERROR([portaudio is required])])
AC_CHECK_LIB([sidplayfp], [main], [], [AC_MSG_ERROR([libsidplayfp is required])])
AC_CHECK_LIB([xmp], [main], [], [AC_MSG_ERROR([xmp is required])])

case $host in
    *mingw* | *cygwin* )
    windows=true
    AC_CHECK_LIB([glu32], [main])
    AC_CHECK_LIB([opengl32], [main])
    AC_CHECK_LIB([iconv], [main])
    AC_CHECK_LIB([SDL2main], [main])
    AC_CHECK_LIB([mingw32], [main])
    ;;
esac

case $host in
    *nux* )
    AC_CHECK_LIB([GL], [main], [], [AC_MSG_ERROR([opengl lib is required])])
    AC_CHECK_LIB([GLU], [main], [], [AC_MSG_ERROR([opengl lib is required])])
    ;;
esac

# Checks for header files.
AC_CHECK_HEADERS([inttypes.h malloc.h stdint.h sys/ioctl.h sys/param.h sys/time.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_C_RESTRICT
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRNLEN
AC_CHECK_FUNCS([memmove memset pow realpath select sqrt strchr strncasecmp strrchr])


AC_OUTPUT
