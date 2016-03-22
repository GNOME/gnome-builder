PKG_CHECK_MODULES(DEVHELP, [libdevhelp-3.0 >= 3.20.0], [enable_devhelp_plugin=yes], [enable_devhelp_plugin=no])
AM_CONDITIONAL(ENABLE_DEVHELP_PLUGIN, test x$enable_devhelp_plugin != xno)
AC_CONFIG_FILES([plugins/devhelp/Makefile])
