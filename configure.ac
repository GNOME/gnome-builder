AC_PREREQ([2.69])


dnl ***********************************************************************
dnl Define Versioning Information
dnl ***********************************************************************
m4_define([builder_major_version],[3])
m4_define([builder_minor_version],[17])
m4_define([builder_micro_version],[0])
m4_define([builder_version],[builder_major_version.builder_minor_version.builder_micro_version])
m4_define([builder_interface_age],[0])
m4_define([builder_bugreport_url],
	  [https://bugzilla.gnome.org/enter_bug.cgi?product=gnome-builder])
m4_define([builder_debug_default],
          [m4_if(m4_eval(builder_minor_version % 2), [1], [yes], [minimum])])


dnl ***********************************************************************
dnl Initialize Autoconf
dnl ***********************************************************************
AC_INIT([gnome-builder],[builder_version],[builder_bugreport_url])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_MACRO_DIR([build/autotools])
AC_CONFIG_AUX_DIR([build])
AC_SUBST(ACLOCAL_AMFLAGS, "-I build/autotools")
AC_CANONICAL_HOST


dnl ***********************************************************************
dnl Make Version Info Available to .in Files
dnl ***********************************************************************
MAJOR_VERSION=builder_major_version
MINOR_VERSION=builder_minor_version
MICRO_VERSION=builder_micro_version
LIBIDE_VERSION=builder_version
AC_SUBST(MAJOR_VERSION)
AC_SUBST(MINOR_VERSION)
AC_SUBST(MICRO_VERSION)
AC_SUBST(LIBIDE_VERSION)


dnl ***********************************************************************
dnl Initialize Automake
dnl ***********************************************************************
AM_SILENT_RULES([yes])
AM_INIT_AUTOMAKE([1.11 foreign subdir-objects tar-ustar no-dist-gzip dist-xz])
AM_MAINTAINER_MODE([enable])


dnl ***********************************************************************
dnl Internationalization
dnl ***********************************************************************
IT_PROG_INTLTOOL([0.50.1])
GETTEXT_PACKAGE=AC_PACKAGE_TARNAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [GETTEXT package name])
AM_GLIB_GNU_GETTEXT


dnl ***********************************************************************
dnl Check for Required Programs
dnl ***********************************************************************
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_CXX
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums])
AC_PATH_PROG([GLIB_COMPILE_RESOURCES], [glib-compile-resources])
AC_HEADER_STDC
PKG_PROG_PKG_CONFIG([0.22])


dnl ***********************************************************************
dnl Ensure C11 is Supported
dnl ***********************************************************************
AX_CHECK_COMPILE_FLAG([-std=gnu11],
                      [CFLAGS="$CFLAGS -std=gnu11"],
                      [AC_MSG_ERROR([C compiler cannot compile GNU C11 code])])


dnl ***********************************************************************
dnl Ensure C++11 is Supported
dnl ***********************************************************************
AX_CXX_COMPILE_STDCXX_11([noext], [mandatory])

dnl ***********************************************************************
dnl Find Path of GNU Make
dnl ***********************************************************************
AC_CHECK_PROG([GMAKE], [gmake], [gmake], [make])
AC_DEFINE_UNQUOTED([GNU_MAKE_NAME], ["$GMAKE"], [Name of GNU make on this system])


dnl ***********************************************************************
dnl Enable high-performance counters
dnl ***********************************************************************
GB_ENABLE_RDTSCP


dnl ***********************************************************************
dnl Setup Debug and Tracing Support
dnl ***********************************************************************
AC_ARG_ENABLE(tracing,
              AS_HELP_STRING([--enable-tracing=@<:@no/yes@:>@],
                             [add extra debugging information @<:@default=no@:>@]),
              ,
              enable_tracing=no)
AS_IF([test "x$enable_tracing" = "xyes"],[enable_debug=yes ENABLE_TRACING=1],[ENABLE_TRACING=0])
AC_SUBST(ENABLE_TRACING)

AC_ARG_ENABLE(debug,
              AS_HELP_STRING([--enable-debug=@<:@no/minimum/yes@:>@],
                             [turn on debugging @<:@default=builder_debug_default@:>@]),
              ,
              enable_debug=builder_debug_default)
AS_CASE(["$enable_debug"],
        [yes],[
            DEBUG_CFLAGS="$DEBUG_CFLAGS -O0"
            DEBUG_CFLAGS="$DEBUG_CFLAGS -g"
        ],
        [minimum],[
            DEBUG_CFLAGS="$DEBUG_CFLAGS -DG_DISABLE_CAST_CHECKS"
        ],
        [no],[
            DEBUG_CFLAGS="$DEBUG_CFLAGS -DG_DISABLE_ASSERT"
            DEBUG_CFLAGS="$DEBUG_CFLAGS -DG_DISABLE_CHECKS"
            DEBUG_CFLAGS="$DEBUG_CFLAGS -DG_DISABLE_CAST_CHECKS"
        ],
        [])
AC_SUBST(DEBUG_CFLAGS)

BUGREPORT_URL=builder_bugreport_url
AC_SUBST(BUGREPORT_URL)


dnl ***********************************************************************
dnl Check for Required Packages
dnl ***********************************************************************
m4_define([gtk_required_version], [3.17.1])
m4_define([glib_required_version], [2.45.1])
m4_define([gtksourceview_required_version], [3.17.2])
m4_define([ggit_required_version], [0.22.9])
m4_define([gjs_required_version], [1.42.0])
m4_define([pygobject_required_version], [3.0.0])
m4_define([devhelp_required_version], [3.16.0])
m4_define([libxml_required_version], [2.9.0])
m4_define([peas_required_version], [1.14.1])

PKG_CHECK_MODULES(BUILDER,  [gtk+-3.0 >= gtk_required_version
                             gio-2.0 >= glib_required_version
                             gtksourceview-3.0 >= gtksourceview_required_version
                             libdevhelp-3.0 >= devhelp_required_version
                             libgit2-glib-1.0 >= ggit_required_version
                             libpeas-1.0 >= peas_required_version])
PKG_CHECK_MODULES(EGG,      [glib-2.0 >= glib_required_version
                             gtk+-3.0 >= gtk_required_version])
PKG_CHECK_MODULES(LIBIDE,   [gio-2.0 >= glib_required_version
                             gio-unix-2.0 >= glib_required_version
                             gtksourceview-3.0 >= gtksourceview_required_version
                             libdevhelp-3.0 >= devhelp_required_version
                             libgit2-glib-1.0 >= ggit_required_version
                             libpeas-1.0 >= peas_required_version
                             gjs-1.0 >= gjs_required_version
                             gjs-internals-1.0 >= gjs_required_version
                             pygobject-3.0 >= pygobject_required_version])
PKG_CHECK_MODULES(RG,       [gtk+-3.0 >= gtk_required_version])
PKG_CHECK_MODULES(SEARCH,   [glib-2.0 >= glib_required_version])
PKG_CHECK_MODULES(GEDIT,    [glib-2.0 >= glib_required_version
                             gtk+-3.0 >= gtk_required_version])
PKG_CHECK_MODULES(NAUTILUS, [glib-2.0 >= glib_required_version
                             gtk+-3.0 >= gtk_required_version])
PKG_CHECK_MODULES(XML,      [gio-2.0 >= glib_required_version
                             libxml-2.0 >= libxml_required_version])


dnl ***********************************************************************
dnl Check for Clang, Required for C/C++ Diagnostics
dnl ***********************************************************************
AC_CHECK_PROGS([LLVM_CONFIG], [llvm-config-3.5 llvm-config35 llvm-config], [])
AS_IF([test "x$LLVM_CONFIG" = "x"],[
	AC_MSG_ERROR([Failed to locate the llvm-config program. Please install llvm-devel.])
])
CLANG_INCLUDEDIR=`$LLVM_CONFIG --includedir`
CLANG_CFLAGS="-I$CLANG_INCLUDEDIR"
CLANG_LDFLAGS=`$LLVM_CONFIG --ldflags`
AC_SUBST([CLANG_INCLUDEDIR])
AC_SUBST([CLANG_CFLAGS])
AC_SUBST([CLANG_LDFLAGS])
CPPFLAGS_SAVE=${CPPFLAGS}
CPPFLAGS=-I${CLANG_INCLUDEDIR}
AC_CHECK_HEADER([clang-c/Index.h], [], [
    AC_MSG_ERROR([Failed to locage clang-c/Index.h. Please install clang-devel.])
])
CPPFLAGS=${CPPFLAGS_SAVE}


dnl ***********************************************************************
dnl Check for Required Python
dnl ***********************************************************************
AC_PATH_TOOL(PYTHON3_CONFIG, "python3-config")
AS_IF([test -z "${PYTHON3_CONFIG}"],[
  AC_MSG_ERROR([Failed to locate python3-config.])
])
LIBIDE_CFLAGS="${LIBIDE_CFLAGS} `${PYTHON3_CONFIG} --includes`"
LIBIDE_LIBS="${LIBIDE_LIBS} `${PYTHON3_CONFIG} --libs`"
LIBIDE_LDFLAGS="${LIBIDE_LDFLAGS} `${PYTHON3_CONFIG} --ldflags`"


dnl ***********************************************************************
dnl Initialize Libtool
dnl ***********************************************************************
LT_PREREQ([2.2])
LT_INIT


dnl ***********************************************************************
dnl Additional C Compiler Flags
dnl ***********************************************************************
AX_APPEND_COMPILE_FLAGS([ \
	-Wall \
	-Wcast-align \
	-Wdeclaration-after-statement \
	-Wempty-body \
	-Werror=format-security \
	-Werror=format=2 \
	-Wextra \
	-Wformat \
	-Wformat-nonliteral \
	-Wformat-security \
	-Winit-self \
	-Wmisleading-indentation \
	-Wmissing-include-dirs \
	-Wshift-negative-value \
	-Wnested-externs \
	-Wno-missing-field-initializers \
	-Wno-sign-compare \
	-Wno-strict-aliasing \
	-Wno-uninitialized \
	-Wno-unused-parameter \
	-Wpointer-arith \
	-Wredundant-decls \
	-Wreturn-type \
	-Wshadow \
	-Wswitch-default \
	-Wswitch-enum \
	-Wundef \
	-Wuninitialized \
])
AC_C_CONST


dnl ***********************************************************************
dnl Desktop Settings Support
dnl ***********************************************************************
GLIB_GSETTINGS


dnl ***********************************************************************
dnl Setup Introspection
dnl ***********************************************************************
GOBJECT_INTROSPECTION_CHECK([1.42.0])


dnl ***********************************************************************
dnl Always build tests
dnl ***********************************************************************
AC_ARG_ENABLE([tests],
              AS_HELP_STRING([--enable-tests],
                             [Enable always building tests during 'make all' [default=no]]),
              [enable_tests=$enableval], [enable_tests=no])

AM_CONDITIONAL([ENABLE_TESTS], test "$enable_tests" = "yes")


dnl ***********************************************************************
dnl Support for gtk-doc Documentation Engine
dnl ***********************************************************************
GTK_DOC_CHECK([1.11],[--flavour no-tmpl])
AM_CONDITIONAL(ENABLE_GTK_DOC, test "x$enable_gtk_doc" = "xyes")
AC_ARG_ENABLE(doc-cross-references,
              AS_HELP_STRING([--disable-doc-cross-references],
                             [cross reference symbols from other libraries @<:@default=yes@:>@]),
              enable_doc_cross_references=$enableval,
              enable_doc_cross_references=yes)
AM_CONDITIONAL(ENABLE_DOC_CROSS_REFERENCES, test x$enable_doc_cross_references != xno)


dnl ***********************************************************************
dnl Rules for Generating AppStream Information
dnl ***********************************************************************
APPSTREAM_XML


dnl ***********************************************************************
dnl Setup Various Compiler Optimizations
dnl ***********************************************************************
AC_MSG_CHECKING([whether to enable optimized builds])
AC_ARG_ENABLE(optimizations, 
              AC_HELP_STRING([--enable-optimizations],
                             [turn on build-time optimizations [default=yes]]),
              [enable_optimizations=$enableval],
              [AS_IF([test "x$enable_debug" = "xyes"],
		     [enable_optimizations="no"],
		     [enable_optimizations="yes"])])
AC_MSG_RESULT([$enable_optimizations])
AC_ARG_ENABLE(lto,
              AC_HELP_STRING([--enable-lto],
                             [turn on link time optimizations [default=no]]),
              [enable_lto=$enableval],
              [enable_lto=no])
OPTIMIZE_CFLAGS=""
OPTIMIZE_LDFLAGS=""
# Enable -Bsymbolic
AS_IF([test "$enable_optimizations" != "no"],
      [AX_CHECK_LINK_FLAG([-Wl,-Bsymbolic],
                          [OPTIMIZE_LDFLAGS="$OPTIMIZE_LDFLAGS -Wl,-Bsymbolic"])
       AX_CHECK_LINK_FLAG([-fno-plt],
                          [OPTIMIZE_LDFLAGS="$OPTIMIZE_LDFLAGS -fno-plt"])])


# Enable Link-Time-Optimization
GB_ENABLE_LTO
OPTIMIZE_CFLAGS="$OPTIMIZE_CFLAGS $LTO_CFLAGS"


AC_SUBST(OPTIMIZE_CFLAGS)
AC_SUBST(OPTIMIZE_LDFLAGS)


dnl ***********************************************************************
dnl Check if shm_open Requires a Library
dnl ***********************************************************************
AC_CHECK_FUNCS([shm_open], [SHM_LIB=],
               [AC_CHECK_LIB([rt], [shm_open], [SHM_LIB=-lrt], [SHM_LIB=])])
AC_SUBST(SHM_LIB)


dnl ***********************************************************************
dnl Check if we should instrument our targets
dnl ***********************************************************************
AC_MSG_CHECKING([for --enable-profiling])
AC_ARG_ENABLE(profiling,
              AC_HELP_STRING([--enable-profiling],
                             [turn on runtime instrumentation [default=no]]),
              [enable_profiling=$enableval],
	      [enable_profiling=no])
AS_IF([test "$enable_profiling" = "yes"],[
	ac_save_cflags="$CFLAGS"
	CFLAGS="$CFLAGS -pg"
	AC_LINK_IFELSE([AC_LANG_SOURCE([[int main() { return 0; }]])],
	               [PROFILING_CFLAGS="-pg"],
		       [enable_profiling=no])
	CFLAGS="$ac_save_cflags $PROFILING_CFLAGS"
])
AC_MSG_RESULT([$enable_profiling])


dnl ***********************************************************************
dnl Process .in Files
dnl ***********************************************************************
AC_CONFIG_FILES([
	Makefile

	build/Makefile
	build/autotools/Makefile

	contrib/Makefile
	contrib/egg/Makefile
	contrib/gedit/Makefile
	contrib/libeditorconfig/Makefile
	contrib/nautilus/Makefile
	contrib/rg/Makefile
	contrib/search/Makefile
	contrib/xml/Makefile

	libide/ide-debug.h
	libide/Makefile

	plugins/Makefile
	plugins/command-bar/Makefile
	plugins/symbol-tree/Makefile

	data/Makefile
	data/gsettings/Makefile
	data/icons/Makefile
	data/libide-1.0.pc
	data/style-schemes/Makefile

	doc/Makefile
	doc/examples/Makefile
	doc/examples/scripts/Makefile
	doc/reference/Makefile
	doc/reference/libide/Makefile
	doc/reference/libide/version.xml

	po/Makefile.in

	src/Makefile

	tests/Makefile

	tools/Makefile
])
AC_OUTPUT


echo ""
echo " ${PACKAGE} - ${VERSION}"
echo ""
echo " Options"
echo ""
echo "  Prefix ............................... : ${prefix}"
echo "  Libdir ............................... : ${libdir}"
echo "  Optimized Build ...................... : ${enable_optimizations}"
echo "  Link-Time-Optimization (experimental). : ${enable_lto}"
echo "  rdtscp ............................... : ${enable_rdtscp}"
echo ""
echo " Development Options"
echo ""
echo "  Enable Debug ......................... : ${enable_debug}"
echo "  Enable Tracing ....................... : ${enable_tracing}"
echo "  Enable Profiling (-pg)................ : ${enable_profiling}"
echo "  Build Test Suite ..................... : ${enable_tests}"
echo "  Build API reference .................. : ${enable_gtk_doc}"
echo "  Use cross-referencing in API docs .... : ${enable_doc_cross_references}"
echo ""
