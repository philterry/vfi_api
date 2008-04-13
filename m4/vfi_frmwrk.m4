AC_DEFUN([VFI_PATH_VFI_FRMWRK],
[ AC_CACHE_CHECK(if we have path to VFI_FRMWRK,vfi_cv_have_vfi_frmwrk,
[ 

vfi_frmwrk_cflags=""
vfi_frmwrk_libs=""

here=`pwd`
cd ${srcdir}
there=`pwd`
cd ${here}

if test -d ${srcdir}/vfi_frmwrk ; then
    vfi_frmwrk_cflags="-I${there}/vfi_api/src"
    vfi_frmwrk_libs="{there}/vfi_api/src/libvfi_frmwrk.la"
else if test -d ${srcdir}/../vfi_api ; then
    vfi_frmwrk_cflags="-I`dirname ${there}`/vfi_api/src"
    vfi_frmwrk_libs="`dirname ${there}`/vfi_api/src/libvfi_frmwrk.la"
else if test -d ${srcdir}/../../vfi_api ; then
    vfi_frmwrk_cflags="-I`dirname \`dirname ${there}\``/vfi_api/src"
    vfi_frmwrk_libs="`dirname \`dirname ${there}\``/vfi_api/src/libvfi_frmwrk.la"
else

AC_ARG_WITH(vfi_frmwrk-prefix,
		[ --with-vfi_frmwrk-prefix=DIR Prefix where the VFI_FRMWRK is installed (optional)],
		vfi_frmwrk_prefix=$withval, vfi_frmwrk_prefix="")
AC_ARG_WITH(vfi_frmwrk-exec-prefix,
		[ --with-vfi_frmwrk-exec-prefix=DIR Exec prefix where the VFI_FRMWRK is installed (optional)],
		vfi_frmwrk_exec_prefix=$withval, vfi_frmwrk_exec_prefix="")

if test "x${vfi_frmwrk_exec_prefix}" != x ; then
	vfi_frmwrkconfig_args="${vfi_frmwrkconfig_args} --exec-prefix=${vfi_frmwrk_exec_prefix}"
	if test "x${VFI_FRMWRKCONFIG+set}" != xset ; then
		VFI_FRMWRKCONFIG="${vfi_frmwrk_exec_prefix}/bin/vfi_frmwrk-config"
	fi
fi 
if test "x${vfi_frmwrk_prefix}" != x ; then
	vfi_frmwrkconfig_args="${vfi_frmwrkconfig_args} --prefix=${vfi_frmwrk_prefix}"
	if test "x${VFI_FRMWRKCONFIG+set}" != xset ; then
		VFI_FRMWRKCONFIG="${vfi_frmwrk_prefix}/bin/vfi_frmwrk-config"
	fi
fi 

AC_PATH_PROG(VFI_FRMWRKCONFIG, vfi_frmwrk-config, nope)

if test "${VFI_FRMWRKCONFIG}" = nope ; then
	vfi_cv_have_vfi_frmwrk="have_vfi_frmwrk=no vfi_frmwrk_cflags=\"\" vfi_frmwrk_libs=\"\""
	AC_MSG_ERROR("Make sure vfi_frmwrk-config is in your path or set VFI_FRMWRKCONFIG or use --with-vfi_frmwrk-[exec]-prefix")
fi

vfi_frmwrk_cflags=`${VFI_FRMWRKCONFIG} ${vfi_frmwrkconfig_args} --cflags`
vfi_frmwrk_libs=`${VFI_FRMWRKCONFIG} ${vfi_frmwrkconfig_args} --libs`

fi
fi
fi

vfi_cv_have_vfi_frmwrk="have_vfi_frmwrk=yes vfi_frmwrk_cflags=\"${vfi_frmwrk_cflags}\" vfi_frmwrk_libs=\"${vfi_frmwrk_libs}\""

])

eval ${vfi_cv_have_vfi_frmwrk}

VFI_FRMWRK_CFLAGS="${vfi_frmwrk_cflags}"
AC_SUBST(VFI_FRMWRK_CFLAGS)
VFI_FRMWRK_LIBS="${vfi_frmwrk_libs}"
AC_SUBST(VFI_FRMWRK_LIBS)

])

