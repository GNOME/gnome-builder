AC_PREREQ([2.69])


dnl ***********************************************************************
dnl Define Versioning Information
dnl ***********************************************************************
m4_define([builder_major_version],[3])
m4_define([builder_minor_version],[19])
m4_define([builder_micro_version],[4])
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
dnl User documentation
dnl ***********************************************************************
YELP_HELP_INIT


dnl ***********************************************************************
dnl Check for Required Programs
dnl ***********************************************************************
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AM_PROG_VALAC([0.30.0.55])
AC_PROG_INSTALL
AM_PROG_LEX
AC_PROG_YACC
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums])
AC_PATH_PROG([GLIB_COMPILE_RESOURCES], [glib-compile-resources])
AC_HEADER_STDC
PKG_PROG_PKG_CONFIG([0.22])
GOBJECT_INTROSPECTION_CHECK([1.42.0])
VAPIGEN_CHECK
GLIB_GSETTINGS
APPSTREAM_XML


dnl ***********************************************************************
dnl Check for Required Headers
dnl ***********************************************************************
AC_ARG_ENABLE([editorconfig],
              AS_HELP_STRING([--enable-editorconfig],
                             [Enable editorconfig file settings.]),
              [enable_editorconfig=$enableval],
              [enable_editorconfig=auto])
AC_CHECK_HEADER([pcre.h],[have_pcre=yes],[have_pcre=no])
AS_IF([test "$enable_editorconfig" = yes && test "$have_pcre" != "yes"],[
       AC_MSG_ERROR([editorconfig requires pcre.h])
])
AS_IF([test "$enable_editorconfig" = "auto" && test "$have_pcre" = "yes"],[
       enable_editorconfig=yes
])
AM_CONDITIONAL([ENABLE_EDITORCONFIG],[test "$enable_editorconfig" = "yes"])


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
AS_IF([test $enable_rdtscp = yes],[
	CFLAGS="$CFLAGS -DEGG_HAVE_RDTSCP"
])
AC_CHECK_FUNCS([sched_getcpu])


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
m4_define([gtk_required_version], [3.19.7])
m4_define([glib_required_version], [2.47.4])
m4_define([gtksourceview_required_version], [3.19.4])
m4_define([gobject_introspection_version], [1.47.1])
m4_define([pygobject_required_version], [3.19.3])
m4_define([libxml_required_version], [2.9.0])
m4_define([pangoft2_required_version], [1.38.0])
m4_define([peas_required_version], [1.17.0])

PKG_CHECK_MODULES(EGG,      [glib-2.0 >= glib_required_version
                             gmodule-2.0 >= glib_required_version
                             gtk+-3.0 >= gtk_required_version])
PKG_CHECK_MODULES(GD,       [gtk+-3.0 >= gtk_required_version])
PKG_CHECK_MODULES(GEDIT,    [glib-2.0 >= glib_required_version
                             gtk+-3.0 >= gtk_required_version])
PKG_CHECK_MODULES(ICONS,    [gio-2.0 >= glib_required_version])
PKG_CHECK_MODULES(LIBIDE,   [gio-2.0 >= glib_required_version
                             gio-unix-2.0 >= glib_required_version
                             gtk+-3.0 >= gtk_required_version
                             gtksourceview-3.0 >= gtksourceview_required_version
                             libpeas-1.0 >= peas_required_version
                             libxml-2.0 >= libxml_required_version
                             pangoft2 >= pangoft2_required_version])
PKG_CHECK_MODULES(NAUTILUS, [glib-2.0 >= glib_required_version
                             gtk+-3.0 >= gtk_required_version])
PKG_CHECK_MODULES(PYGOBJECT,[pygobject-3.0 >= pygobject_required_version],
                            [have_pygobject=yes],
                            [have_pygobject=no])
PKG_CHECK_MODULES(RG,       [gtk+-3.0 >= gtk_required_version])
PKG_CHECK_MODULES(SEARCH,   [glib-2.0 >= glib_required_version])
PKG_CHECK_MODULES(TMPL,     [gio-2.0 >= glib_required_version
			     gobject-introspection-1.0 >= gobject_introspection_version])
PKG_CHECK_MODULES(XML,      [gio-2.0 >= glib_required_version
                             libxml-2.0 >= libxml_required_version])


dnl ***********************************************************************
dnl Check for Required Python
dnl ***********************************************************************
enable_python_scripting=no
AS_IF([test "x$have_pygobject" = "xyes"],[
	AM_PATH_PYTHON([3.2.3])
	AC_PATH_TOOL(PYTHON3_CONFIG, "python3-config")
	AS_IF([test -z "${PYTHON3_CONFIG}"],[
		AC_MSG_RESULT([Failed to locate python3-config.])
	],[
		LIBIDE_CFLAGS="${LIBIDE_CFLAGS} `${PYTHON3_CONFIG} --includes`"
		LIBIDE_LIBS="${LIBIDE_LIBS} `${PYTHON3_CONFIG} --libs`"
		LIBIDE_LDFLAGS="${LIBIDE_LDFLAGS} `${PYTHON3_CONFIG} --ldflags`"
		enable_python_scripting=yes
	])

	pyoverridesdir="\$(pyexecdir)/gi/overrides"
	AC_SUBST(pyoverridesdir)
])


dnl ***********************************************************************
dnl Check for supported plugins
dnl ***********************************************************************
m4_include([plugins/autotools/configure.ac])
m4_include([plugins/build-tools/configure.ac])
m4_include([plugins/c-pack/configure.ac])
m4_include([plugins/clang/configure.ac])
m4_include([plugins/command-bar/configure.ac])
m4_include([plugins/contributing/configure.ac])
m4_include([plugins/create-project/configure.ac])
m4_include([plugins/ctags/configure.ac])
m4_include([plugins/devhelp/configure.ac])
m4_include([plugins/file-search/configure.ac])
m4_include([plugins/fpaste/configure.ac])
m4_include([plugins/gcc/configure.ac])
m4_include([plugins/gettext/configure.ac])
m4_include([plugins/git/configure.ac])
m4_include([plugins/gnome-code-assistance/configure.ac])
m4_include([plugins/html-completion/configure.ac])
m4_include([plugins/html-preview/configure.ac])
m4_include([plugins/jedi/configure.ac])
m4_include([plugins/jhbuild/configure.ac])
m4_include([plugins/library-template/configure.ac])
m4_include([plugins/mingw/configure.ac])
m4_include([plugins/project-tree/configure.ac])
m4_include([plugins/python-gi-imports-completion/configure.ac])
m4_include([plugins/python-pack/configure.ac])
m4_include([plugins/support/configure.ac])
m4_include([plugins/symbol-tree/configure.ac])
m4_include([plugins/sysmon/configure.ac])
m4_include([plugins/todo/configure.ac])
m4_include([plugins/terminal/configure.ac])
m4_include([plugins/vala-pack/configure.ac])
m4_include([plugins/xdg-app/configure.ac])
m4_include([plugins/xml-pack/configure.ac])


dnl ***********************************************************************
dnl Define automake conditionals based on what we found
dnl ***********************************************************************
AM_CONDITIONAL(ENABLE_PYTHON_SCRIPTING, test x$enable_python_scripting != xno)


dnl ***********************************************************************
dnl Initialize Libtool
dnl ***********************************************************************
LT_PREREQ([2.2])
LT_INIT
LT_LIB_M


dnl ***********************************************************************
dnl Additional C Compiler Flags
dnl ***********************************************************************
AX_CHECK_COMPILE_FLAG([-Werror=unknown-warning-option], [
	ax_compiler_flags_test="-Werror=unknown-warning-option"
], [
	ax_compiler_flags_test=""
])
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
], [], [$ax_compiler_flags_test])
AC_C_CONST


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
OPTIMIZE_CFLAGS=""
OPTIMIZE_LDFLAGS=""
# Enable -Bsymbolic
AS_IF([test "$enable_optimizations" != "no"],
      [AX_CHECK_LINK_FLAG([-Wl,-Bsymbolic],
                          [OPTIMIZE_LDFLAGS="$OPTIMIZE_LDFLAGS -Wl,-Bsymbolic"])
       AX_CHECK_LINK_FLAG([-fno-plt],
                          [OPTIMIZE_LDFLAGS="$OPTIMIZE_LDFLAGS -fno-plt"])])
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
	contrib/gd/Makefile
	contrib/libeditorconfig/Makefile
	contrib/nautilus/Makefile
	contrib/rg/Makefile
	contrib/search/Makefile
	contrib/tmpl/Makefile
	contrib/tmpl/tmpl-debug.h
	contrib/xml/Makefile

	libide/ide-debug.h
	libide/Makefile

	plugins/Makefile

	data/Makefile
	data/fonts/Makefile
	data/gsettings/org.gnome.builder.code-insight.gschema.xml
	data/gsettings/Makefile
	data/icons/Makefile
	data/icons/hicolor/Makefile
	data/libide-1.0.pc
	data/style-schemes/Makefile
	data/template-glib-1.0.pc

	doc/Makefile
	doc/examples/Makefile
	doc/examples/scripts/Makefile
	doc/reference/Makefile
	doc/reference/libide/Makefile
	doc/reference/libide/version.xml

	help/Makefile

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
echo " Scripting"
echo ""
echo "  Python ............................... : ${enable_python_scripting}"
echo ""
echo " Plugins"
echo ""
echo "  Autotools ............................ : ${enable_autotools_plugin}"
echo "  Build Tools .......................... : ${enable_build_tools_plugin}"
echo "  C Language Pack ...................... : ${enable_c_pack_plugin}"
echo "  Clang ................................ : ${enable_clang_plugin}"
echo "  Command Bar .......................... : ${enable_command_bar_plugin}"
echo "  Contribute ........................... : ${enable_contributing_plugin}"
echo "  Ctags ................................ : ${enable_ctags_plugin}"
echo "  Devhelp .............................. : ${enable_devhelp_plugin}"
echo "  Editorconfig ......................... : ${enable_editorconfig}"
echo "  Fpaste.org ........................... : ${enable_fpaste_plugin}"
echo "  GCC .................................. : ${enable_gcc_plugin}"
echo "  Gettext .............................. : ${enable_gettext_plugin}"
echo "  Git Version Control .................. : ${enable_git_plugin}"
echo "  Global File Search ................... : ${enable_file_search_plugin}"
echo "  GNOME Code Assistance ................ : ${enable_gnome_code_assistance_plugin}"
echo "  HTML Autocompletion .................. : ${enable_html_completion_plugin}"
echo "  HTML and Markdown Preview ............ : ${enable_html_preview_plugin}"
echo "  JHBuild .............................. : ${enable_jhbuild_plugin}"
echo "  MinGW ................................ : ${enable_mingw_plugin}"
echo "  Project Creation ..................... : ${enable_create_project_plugin}"
echo "  Project Tree ......................... : ${enable_project_tree_plugin}"
echo "  Python GObject Introspection ......... : ${enable_python_gi_imports_completion_plugin}"
echo "  Python Jedi Autocompletion ........... : ${enable_jedi_plugin}"
echo "  Python Language Pack ................. : ${enable_python_pack_plugin}"
echo "  Support .............................. : ${enable_support_plugin}"
echo "  System Monitor ....................... : ${enable_sysmon_plugin}"
echo "  Symbol Tree .......................... : ${enable_symbol_tree_plugin}"
echo "  Todo ................................. : ${enable_todo_plugin}"
echo "  Terminal ............................. : ${enable_terminal_plugin}"
echo "  Vala Language Pack ................... : ${enable_vala_pack_plugin}"
echo "  Xdg-App .............................. : ${enable_xdg_app_plugin}"
echo "  XML Language Pack .................... : ${enable_xml_pack_plugin}"
echo ""
echo " Templates"
echo ""
echo "  Library .............................. : ${enable_library_template_plugin}"
echo ""

