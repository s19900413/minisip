# AM_MINISIP_CHECK_LIBMIKEY(VERSION)
# ----------------------------------
AC_DEFUN([AM_MINISIP_CHECK_LIBMIKEY],[ 
	AC_REQUIRE([AM_MINISIP_CHECK_LIBMUTIL]) dnl
dnl	AC_REQUIRE([AM_MINISIP_CHECK_LIBMCRYPTO]) dnl
	AC_MINISIP_WITH_ARG(MIKEY, mikey, libmikey, $1, [REQUIRED])
	AC_MINISIP_CHECK_LIBRARY(MIKEY, libmikey, libmikey_config.h, mikey)
  ])
# End of AM_MINISIP_CHECK_LIBMIKEY
#