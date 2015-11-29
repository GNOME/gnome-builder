PKG_CHECK_MODULES(GIT, [libgit2-glib-1.0 >= 0.23.7], [enable_git_plugin=yes], [enable_git_plugin=no])
AM_CONDITIONAL(ENABLE_GIT_PLUGIN, test x$enable_git_plugin != xno)
AC_CONFIG_FILES([plugins/git/Makefile])
