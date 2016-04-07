m4_define([gtkmm_required_version], [3.19.12])

AC_SUBST([IDEMM_SO_VERSION], [0:0:0])

MM_PREREQ([0.9.8])
MM_INIT_MODULE([idemm-1.0])

MM_CONFIG_DOCTOOL_DIR([docs])

MM_ARG_ENABLE_DOCUMENTATION

MM_PKG_CONFIG_SUBST([GMMPROC_DIR], [--variable=gmmprocdir glibmm-2.4])
MM_PKG_CONFIG_SUBST([GMMPROC_EXTRA_M4_DIR], [--variable=gmmprocm4dir cairomm-1.0 pangomm-1.4 atkmm-1.6 gtkmm-3.0])

PKG_CHECK_MODULES(IDEMM,    [gtkmm-3.0 >= gtkmm_required_version])


AC_ARG_ENABLE([idemm],
              [AS_HELP_STRING([--enable-idemm=@<:@yes/no@:>@],
                              [Build idemm library (C++ wrapper for libide])],
              [enable_idemm=$enableval],
	      [enable_idemm=auto])

AS_IF([test x$enable_idemm != xno],[
        PKG_CHECK_MODULES(GTKMM,
	                  [gtkmm-3.0 >= gtkmm_required_version],
			  [have_gtkmm=yes],
			  [have_gtkmm=no])

         AS_IF([test x$enable_idemm = xyes && x$have_idemm = xno],[
	        AC_MSG_ERROR([--enable-idemm requires gtkmm >= gtkmm_required_version])
	])

	enable_idemm=$have_gtkmm
])

AM_CONDITIONAL(ENABLE_IDEMM, test x$enable_idemm = xyes)

AC_CONFIG_HEADERS([libidemm/idemmconfig.h])

AC_CONFIG_FILES([libidemm/Makefile])
AC_CONFIG_FILES([libidemm/idemm/Makefile])
AC_CONFIG_FILES([libidemm/src/Makefile])
AC_CONFIG_FILES([libidemm/tools/Makefile])
