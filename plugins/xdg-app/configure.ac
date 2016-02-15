m4_define([xdg_app_required_version], [0.4.8])

# --enable-xdg-app-plugin=yes/no/auto
AC_ARG_ENABLE([xdg-app-plugin],
              [AS_HELP_STRING([--enable-xdg-app-plugin=@<:@yes/no/auto@:>@],
                              [Build with support for xdg-app.])],
              [enable_xdg_app_plugin=$enableval],
              [enable_xdg_app_plugin=auto])

AS_IF([test "$enable_xdg_app_plugin" != no],[
	PKG_CHECK_MODULES(XDG_APP,
	                  [xdg-app >= xdg_app_required_version],
	                  [have_xdg_app=yes],
	                  [have_xdg_app=no])

	AS_IF([test "$enable_xdg_app_plugin" = "yes" && "$have_xdg_app" = "no"],[
	       AC_MSG_ERROR([--enable-xdg-app-plugin requires xdg-app >= xdg_app_required_version])
	])

	enable_xdg_app_plugin=yes
])

AM_CONDITIONAL(ENABLE_XDG_APP_PLUGIN, test x$enable_xdg_app_plugin = xyes)
AC_CONFIG_FILES([plugins/xdg-app/Makefile])
