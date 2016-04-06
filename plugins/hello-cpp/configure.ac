m4_define([gtkmm_required_version], [3.18.0])

# --enable-hello-cpp-plugin=yes/no
AC_ARG_ENABLE([hello-cpp-plugin],
              [AS_HELP_STRING([--enable-hello-cpp-plugin=@<:@yes/no@:>@],
                              [Build example C++ plugin with gtkmm.])],
              [enable_hello_cpp_plugin=$enableval],
              [enable_hello_cpp_plugin=auto])

AS_IF([test x$enable_hello_cpp_plugin != x$no],[
	PKG_CHECK_MODULES(GTKMM,
	                  [gtkmm-3.0 >= gtkmm_required_version],
	                  [have_gtkmm=yes],
	                  [have_gtkmm=no])

	AS_IF([test x$enable_hello_cpp_plugin = xyes && x$have_gtkmm = xno],[
	       AC_MSG_ERROR([--enable-hello-cpp-plugin requires gtkmm-3.0 >= gtkmm_required_version])
	])

	enable_hello_cpp_plugin=$have_gtkmm
])

AM_CONDITIONAL(ENABLE_HELLO_CPP_PLUGIN, test x$enable_hello_cpp_plugin = xyes)
AC_CONFIG_FILES([plugins/hello-cpp/Makefile])
