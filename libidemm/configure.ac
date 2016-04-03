m4_define([glibmm_required_version], [2.47.3])
m4_define([gtkmm_required_version], [3.19.12])

AC_SUBST([LIBIDEMM_SO_VERSION], [0:0:0])

MM_PREREQ([0.9.8])
MM_INIT_MODULE([libidemm-1.0])

MM_CONFIG_DOCTOOL_DIR([docs])

MM_ARG_ENABLE_DOCUMENTATION

MM_PKG_CONFIG_SUBST([GMMPROC_DIR], [--variable=gmmprocdir glibmm-2.4])
MM_PKG_CONFIG_SUBST([GMMPROC_EXTRA_M4_DIR], [--variable=gmmprocm4dir cairomm-1.0 pangomm-1.4 atkmm-1.6 gtkmm-3.0])

PKG_CHECK_MODULES(LIBIDEMM,  [glibmm-2.4 >= glibmm_required_version
                             gtkmm-3.0 >= gtkmm_required_version])


AC_ARG_ENABLE([libidemm],
              [AS_HELP_STRING([--enable-libidemm=@<:@yes/no@:>@],
                              [Build libidemm library (C++ wrapper for libide])],
              [enable_libidemm=$enableval],
	      [enable_libidemm=auto])

AS_IF([test x$enable_libidemm != xno],[
        PKG_CHECK_MODULES(GTKMM,
	                  [gtkmm-3.0 >= gtkmm_required_version],
			  [have_gtkmm=yes],
			  [have_gtkmm=no])

         AS_IF([test x$enable_libidemm = xyes && x$have_libidemm = xno],[
	        AC_MSG_ERROR([--enable-libidemm requires gtkmm >= gtkmm_required_version])
	])

	enable_libidemm=$have_gtkmm
])

AM_CONDITIONAL(ENABLE_LIBIDEMM, test x$enable_libidemm = xyes)

AC_CONFIG_HEADERS([libidemm/idemmconfig.h])

AC_CONFIG_FILES([libidemm/Makefile])
AC_CONFIG_FILES([libidemm/libidemm/Makefile])
AC_CONFIG_FILES([libidemm/src/Makefile])
AC_CONFIG_FILES([libidemm/tools/Makefile])
