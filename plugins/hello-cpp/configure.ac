# --enable-hello-cpp-plugin=yes/no

AC_ARG_ENABLE([hello-cpp-plugin],
              [AS_HELP_STRING([--enable-hello-cpp-plugin=@<:@yes/no@:>@],
                              [Build example C++ plugin with gtkmm.])],
              [enable_hello_cpp_plugin=$enableval],
              [enable_hello_cpp_plugin=no])

AS_IF([test x$enable_hello_cpp_plugin != xno],[
   	AM_COND_IF([ENABLE_IDEMM], [have_idemm=yes], [have_idemm=no])

	AS_IF([test x$enable_hello_cpp_plugin = xyes && x$have_idemm = xno],[
	       AC_MSG_ERROR([--enable-hello-cpp-plugin requires --enable-idemm=yes])
	])

	enable_hello_cpp_plugin=$have_idemm		
])

AM_CONDITIONAL(ENABLE_HELLO_CPP_PLUGIN, test x$enable_hello_cpp_plugin = xyes)
AC_CONFIG_FILES([plugins/hello-cpp/Makefile])
