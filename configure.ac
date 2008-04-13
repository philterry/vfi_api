#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([vfi_api], [0.1], [Phil Terry <pterry@vmetro.com>])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/vfi_api.c])
AC_CONFIG_HEADER([config.h])
AC_PROG_LIBTOOL
# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
GTK_DOC_CHECK([1.0])

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE

# Checks for library functions.

VFI_API_INCLUDEDIR="-I${includedir}"
AC_SUBST(VFI_API_INCLUDEDIR)
VFI_API_CFLAGS=""
AC_SUBST(VFI_API_CFLAGS)
VFI_API_LIBDIR="-L${libdir}"
AC_SUBST(VFI_API_LIBDIR)
VFI_API_LIBS="-l${PACKAGE}"
AC_SUBST(VFI_API_LIBS)

VFI_FRMWRK_INCLUDEDIR="-I${includedir}"
AC_SUBST(VFI_FRMWRK_INCLUDEDIR)
VFI_FRMWRK_CFLAGS=""
AC_SUBST(VFI_FRMWRK_CFLAGS)
VFI_FRMWRK_LIBDIR="-L${libdir}"
AC_SUBST(VFI_FRMWRK_LIBDIR)
VFI_FRMWRK_LIBS="-l${PACKAGE}"
AC_SUBST(VFI_FRMWRK_LIBS)

AC_CONFIG_FILES([Makefile
                 src/Makefile doc/Makefile m4/Makefile vfi_frmwrk-config libvfi_frmwrk.pc vfi_api-config libvfi_api.pc])
AC_OUTPUT
