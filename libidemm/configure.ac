m4_define([glibmm_required_version], [2.47.3])

AC_ARG_ENABLE([libidemm],
              [AS_HELP_STRING([--enable-libidemm=@<:@yes/no@:>@],
                              [Build libidemm library (C++ wrapper for libide])],
              [enable_libidemm=$enableval],
	      [enable_libidemm=auto])

AS_IF([test x$enable_libidemm != xno],[
        PKG_CHECK_MODULES(GLIBMM,
	                  [glibmm-2.4 >= glibmm_required_version],
			  [have_glibmm=yes],
			  [have_glibmm=no])

         AS_IF([test x$enable_libidemm = xyes && x$have_libidemm = xno],[
	        AC_MSG_ERROR([--enable-libidemm requires glibmm >= glibmm_required_version])
	])

	enable_libidemm=$have_glibmm
])

AM_CONDITIONAL(ENABLE_LIBIDEMM, test x$enable_libidemm = xyes)

AC_CONFIG_FILES([libidemm/Makefile])
AC_CONFIG_FILES([libidemm/libidemm/Makefile])
AC_CONFIG_FILES([libidemm/src/Makefile])
AC_CONFIG_FILES([libidemm/tools/Makefile])
