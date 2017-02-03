m4_define([flatpak_required_version], [0.8.0])
m4_define([ggit_required_version], [0.24.0])

# --enable-flatpak-plugin=yes/no/auto
AC_ARG_ENABLE([flatpak-plugin],
              [AS_HELP_STRING([--enable-flatpak-plugin=@<:@yes/no/auto@:>@],
                              [Build with support for flatpak.])],
              [enable_flatpak_plugin=$enableval],
              [enable_flatpak_plugin=auto])

AS_IF([test "$enable_flatpak_plugin" != no],[
	PKG_CHECK_MODULES(FLATPAK,
	                  [flatpak >= flatpak_required_version
	                   json-glib-1.0 >= json_glib_required_version],
	                  [have_flatpak=yes],
	                  [have_flatpak=no])

	PKG_CHECK_MODULES(SOUP, [libsoup-2.4])


       AS_IF([test "$enable_flatpak_plugin" = "yes" && test "$have_flatpak" = "no"],[
	       AC_MSG_ERROR([--enable-flatpak-plugin requires flatpak >= flatpak_required_version])
	])

	PKG_CHECK_MODULES(GIT,
			  [libgit2-glib-1.0 >= ggit_required_version],
			  [have_git=yes],
			  [have_git=no])

	AS_IF([test "$enable_flatpak_plugin" = "yes" && test "$have_git" = "no"],[
		AC_MSG_ERROR([--enable-flatpak-plugin requires libgit2-glib-1.0 >= ggit_required_version])
	])

	AS_IF([test "$have_flatpak" = "yes" && test "$have_git" = "yes"],[
		[enable_flatpak_plugin=yes]
	])

	dnl ***********************************************************************
	dnl Be extra careful about libgit2-glib requirements
	dnl ***********************************************************************
	cflags_save="${CFLAGS}"
	libs_save="${LIBS}"
	CFLAGS="${CFLAGS} ${GIT_CFLAGS}"
	LIBS="${LIBS} ${GIT_LIBS}"
	# Thread-Safe
	AC_MSG_CHECKING([for thread-safe support in libgit2])
	AC_TRY_RUN([
		#include <libgit2-glib/ggit.h>
		int
		main(int argc, const char *argv[])
		{
			ggit_init ();
			return ((ggit_get_features() & GGIT_FEATURE_THREADS) != 0) ? 0 : 1;
		}
	],[AC_MSG_RESULT([yes])],[AC_MSG_ERROR([no, please recompile a threadsafe version of libgit2 (-DTHREADSAFE:BOOL=ON)])])
	# SSH
	AC_MSG_CHECKING([for SSH support in libgit2])
	AC_TRY_RUN([
		#include <libgit2-glib/ggit.h>
		int
		main(int argc, const char *argv[])
		{
			ggit_init ();
			return ((ggit_get_features() & GGIT_FEATURE_SSH) != 0) ? 0 : 1;
		}
	],[AC_MSG_RESULT([yes])],[AC_MSG_ERROR([no, please recompile a libgit2 with ssh support])])
	CFLAGS="${cflags_save}"
	LIBS="${libs_save}"
])

AM_CONDITIONAL(ENABLE_FLATPAK_PLUGIN, [test "x$enable_flatpak_plugin" = "xyes"])
AC_CONFIG_FILES([plugins/flatpak/Makefile])
