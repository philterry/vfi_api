AC_DEFUN([MMC_PATH_RDDMA_API],
[ AC_CACHE_CHECK(if we have path to RDMA_API,mmc_cv_have_rddma_api,
[ 

rddma_api_cflags=""
rddma_api_libs=""

here=`pwd`
cd ${srcdir}
there=`pwd`
cd ${here}

if test -d ${srcdir}/rddma_api ; then
    rddma_api_cflags="-I${there}/rddma_api/src"
    rddma_api_libs="{there}/rddma_api/src/librddma_api.la"
else if test -d ${srcdir}/../rddma_api ; then
    rddma_api_cflags="-I`dirname ${there}`/rddma_api/src"
    rddma_api_libs="`dirname ${there}`/rddma_api/src/librddma_api.la"
else if test -d ${srcdir}/../../mmc_rddma_api ; then
    rddma_api_cflags="-I`dirname \`dirname ${there}\``/rddma_api/src"
    rddma_api_libs="`dirname \`dirname ${there}\``/rddma_api/src/librddma_api.la"
else

AC_ARG_WITH(rddma_api-prefix,
		[ --with-rddma_api-prefix=DIR Prefix where the RDDMA_API is installed (optional)],
		rddma_api_prefix=$withval, rddma_api_prefix="")
AC_ARG_WITH(rddma_api-exec-prefix,
		[ --with-rddma_api-exec-prefix=DIR Exec prefix where the RDDMA_API is installed (optional)],
		rddma_api_exec_prefix=$withval, rddma_api_exec_prefix="")

if test "x${rddma_api_exec_prefix}" != x ; then
	rddma_apiconfig_args="${rddma_apiconfig_args} --exec-prefix=${rddma_api_exec_prefix}"
	if test "x${RDDMA_APICONFIG+set}" != xset ; then
		RDDMA_APICONFIG="${rddma_api_exec_prefix}/bin/mmcrddma_api-config"
	fi
fi 
if test "x${rddma_api_prefix}" != x ; then
	rddma_apiconfig_args="${rddma_apiconfig_args} --prefix=${rddma_api_prefix}"
	if test "x${RDDMA_APICONFIG+set}" != xset ; then
		RDDMA_APICONFIG="${rddma_api_prefix}/bin/mmcrddma_api-config"
	fi
fi 

AC_PATH_PROG(RDDMA_APICONFIG, mmcrddma_api-config, nope)

if test "${RDDMA_APICONFIG}" = nope ; then
	mmc_cv_have_rddma_api="have_rddma_api=no rddma_api_cflags=\"\" rddma_api_libs=\"\""
	AC_MSG_ERROR("Make sure mmcrddma_api-config is in your path or set RDDMA_APICONFIG or use --with-rddma_api-[exec]-prefix")
fi

rddma_api_cflags=`${RDDMA_APICONFIG} ${rddma_apiconfig_args} --cflags`
rddma_api_libs=`${RDDMA_APICONFIG} ${rddma_apiconfig_args} --libs`

fi
fi
fi

mmc_cv_have_rddma_api="have_rddma_api=yes rddma_api_cflags=\"${rddma_api_cflags}\" rddma_api_libs=\"${rddma_api_libs}\""

])

eval ${mmc_cv_have_rddma_api}

RDDMA_API_CFLAGS="${rddma_api_cflags}"
AC_SUBST(RDDMA_API_CFLAGS)
RDDMA_API_LIBS="${rddma_api_libs}"
AC_SUBST(RDDMA_API_LIBS)

])

