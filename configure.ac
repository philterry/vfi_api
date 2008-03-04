#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([rddma_api], [0.1], [Phil Terry <pterry@micromemory.com>])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/rddma_api.c])
AC_CONFIG_HEADER([config.h])
AC_PROG_LIBTOOL
# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE

# Checks for library functions.

RDDMA_API_INCLUDEDIR="-I${includedir}"
AC_SUBST(RDDMA_API_INCLUDEDIR)
RDDMA_API_CFLAGS=""
AC_SUBST(RDDMA_API_CFLAGS)
RDDMA_API_LIBDIR="-L${libdir}"
AC_SUBST(RDDMA_API_LIBDIR)
RDDMA_API_LIBS="-l${PACKAGE}"
AC_SUBST(RDDMA_API_LIBS)
AC_CONFIG_FILES([Makefile
                 src/Makefile doc/Makefile m4/Makefile mmcrddma_api-config librddma_api.pc])
AC_OUTPUT
