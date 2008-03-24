AC_DEFUN([VFI_PATH_VFI_API],
[ AC_CACHE_CHECK(if we have path to VFI_API,vfi_cv_have_vfi_api,
[ 

vfi_api_cflags=""
vfi_api_libs=""

here=`pwd`
cd ${srcdir}
there=`pwd`
cd ${here}

if test -d ${srcdir}/vfi_api ; then
    vfi_api_cflags="-I${there}/vfi_api/src"
    vfi_api_libs="{there}/vfi_api/src/libvfi_api.la"
else if test -d ${srcdir}/../vfi_api ; then
    vfi_api_cflags="-I`dirname ${there}`/vfi_api/src"
    vfi_api_libs="`dirname ${there}`/vfi_api/src/libvfi_api.la"
else if test -d ${srcdir}/../../vfi_api ; then
    vfi_api_cflags="-I`dirname \`dirname ${there}\``/vfi_api/src"
    vfi_api_libs="`dirname \`dirname ${there}\``/vfi_api/src/libvfi_api.la"
else

AC_ARG_WITH(vfi_api-prefix,
		[ --with-vfi_api-prefix=DIR Prefix where the VFI_API is installed (optional)],
		vfi_api_prefix=$withval, vfi_api_prefix="")
AC_ARG_WITH(vfi_api-exec-prefix,
		[ --with-vfi_api-exec-prefix=DIR Exec prefix where the VFI_API is installed (optional)],
		vfi_api_exec_prefix=$withval, vfi_api_exec_prefix="")

if test "x${vfi_api_exec_prefix}" != x ; then
	vfi_apiconfig_args="${vfi_apiconfig_args} --exec-prefix=${vfi_api_exec_prefix}"
	if test "x${VFI_APICONFIG+set}" != xset ; then
		VFI_APICONFIG="${vfi_api_exec_prefix}/bin/vfi_api-config"
	fi
fi 
if test "x${vfi_api_prefix}" != x ; then
	vfi_apiconfig_args="${vfi_apiconfig_args} --prefix=${vfi_api_prefix}"
	if test "x${VFI_APICONFIG+set}" != xset ; then
		VFI_APICONFIG="${vfi_api_prefix}/bin/vfi_api-config"
	fi
fi 

AC_PATH_PROG(VFI_APICONFIG, vfi_api-config, nope)

if test "${VFI_APICONFIG}" = nope ; then
	vfi_cv_have_vfi_api="have_vfi_api=no vfi_api_cflags=\"\" vfi_api_libs=\"\""
	AC_MSG_ERROR("Make sure vfi_api-config is in your path or set VFI_APICONFIG or use --with-vfi_api-[exec]-prefix")
fi

vfi_api_cflags=`${VFI_APICONFIG} ${vfi_apiconfig_args} --cflags`
vfi_api_libs=`${VFI_APICONFIG} ${vfi_apiconfig_args} --libs`

fi
fi
fi

vfi_cv_have_vfi_api="have_vfi_api=yes vfi_api_cflags=\"${vfi_api_cflags}\" vfi_api_libs=\"${vfi_api_libs}\""

])

eval ${vfi_cv_have_vfi_api}

VFI_API_CFLAGS="${vfi_api_cflags}"
AC_SUBST(VFI_API_CFLAGS)
VFI_API_LIBS="${vfi_api_libs}"
AC_SUBST(VFI_API_LIBS)

])

