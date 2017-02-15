m4_define([ggit_required_version], [0.25.0])

# --enable-git-plugin=yes/no/auto
AC_ARG_ENABLE([git-plugin],
              [AS_HELP_STRING([--enable-git-plugin=@<:@yes/no/auto@:>@],
                              [Build with support for git.])],
              [enable_git_plugin=$enableval],
              [enable_git_plugin=auto])

PKG_CHECK_MODULES(GIT,
                  [libgit2-glib-1.0 >= ggit_required_version],
                  [have_git=yes],
                  [have_git=no])

AS_IF([test "$enable_git_plugin" == "yes" && test "$have_git" == "no"],[
	AC_MSG_ERROR([--enable-git-plugin requires libgit2-glib-1.0 >= ggit_required_version])
])

AS_IF([test "$enable_git_plugin" != "no" && test "$have_git" == "yes"],[
	enable_git_plugin=yes

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
],[enable_git_plugin=no])

AM_CONDITIONAL(ENABLE_GIT_PLUGIN, test x$enable_git_plugin = xyes)
AC_CONFIG_FILES([plugins/git/Makefile])
