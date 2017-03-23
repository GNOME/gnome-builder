AC_PREREQ([2.69])


dnl ***********************************************************************
dnl Define Versioning Information
dnl ***********************************************************************
m4_define([builder_major_version],[3])
m4_define([builder_minor_version],[25])
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
AC_INIT([gnome-builder],
        [builder_version],
        [builder_bugreport_url],
        [gnome-builder],
        [https://wiki.gnome.org/Apps/Builder])
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
dnl Check if we are building from a git checkout
dnl ***********************************************************************
building_from_git=no
AS_IF([test -d "${srcdir}/.git"],[
       building_from_git=yes
       COMMIT_ID=`git describe --always | cut -f 2- -d -`
       AC_DEFINE_UNQUOTED([COMMIT_ID], ["$COMMIT_ID"], [git commit suffix])
       AC_SUBST(COMMIT_ID)
])


dnl ***********************************************************************
dnl Initialize Automake
dnl ***********************************************************************
AM_SILENT_RULES([yes])
AM_INIT_AUTOMAKE([1.11 foreign subdir-objects tar-ustar no-dist-gzip dist-xz])
AM_MAINTAINER_MODE([enable])


dnl ***********************************************************************
dnl Internationalization
dnl ***********************************************************************
GETTEXT_PACKAGE=AC_PACKAGE_TARNAME
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [GETTEXT package name])
AC_SUBST(GETTEXT_PACKAGE)
AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT([external])


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

AC_ARG_VAR([DESKTOP_FILE_VALIDATE], [the desktop-file-validate program])
AC_PATH_PROG([DESKTOP_FILE_VALIDATE], [desktop-file-validate], [])
if test -z "$DESKTOP_FILE_VALIDATE"; then
	AC_MSG_ERROR([desktop-file-validate not found])
fi


dnl ***********************************************************************
dnl Setup the "channel" name to help us track down user issues
dnl ***********************************************************************
channel_guess=distro
AS_IF([test -d "${srcdir}/.git"],[channel_guess=devel])
AS_IF([test x$UNDER_JHBUILD != x],[channel_guess=jhbuild])
AC_ARG_WITH([channel],
            AS_HELP_STRING([--enable-channel=@<:@devel/distro/jhbuild/flatpak-nightly/flatpak-stable@:>@],
                           [Sets the distribution channel for the build to help developers track down bugs]),
            [channel=$withval],
	    [channel=$channel_guess])
AC_DEFINE_UNQUOTED([CHANNEL], ["$channel"], [Distribution channel for Builder])


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
m4_define([gtk_required_version], [3.22.1])
m4_define([glib_required_version], [2.50.0])
m4_define([gtksourceview_required_version], [3.22.0])
m4_define([gobject_introspection_version], [1.48.0])
m4_define([pygobject_required_version], [3.22.0])
m4_define([libxml_required_version], [2.9.0])
m4_define([pangoft2_required_version], [1.38.0])
m4_define([peas_required_version], [1.18.0])
m4_define([json_glib_required_version], [1.2.0])
m4_define([gspell_required_version], [1.2.0])
m4_define([enchant_required_version], [1.6.0])

PKG_CHECK_MODULES(EGG,      [glib-2.0 >= glib_required_version
                             gmodule-2.0 >= glib_required_version
                             gtk+-3.0 >= gtk_required_version])
PKG_CHECK_MODULES(GD,       [gtk+-3.0 >= gtk_required_version])
PKG_CHECK_MODULES(GEDIT,    [glib-2.0 >= glib_required_version
                             gtk+-3.0 >= gtk_required_version])
PKG_CHECK_MODULES(GSTYLE,   [glib-2.0 >= glib_required_version
                             gtk+-3.0 >= gtk_required_version
                             gtksourceview-3.0 >= gtksourceview_required_version])
PKG_CHECK_MODULES(ICONS,    [gio-2.0 >= glib_required_version])
PKG_CHECK_MODULES(JSONRPC,  [json-glib-1.0 >= json_glib_required_version
                             gio-unix-2.0])
PKG_CHECK_MODULES(LIBIDE,   [gio-2.0 >= glib_required_version
                             gio-unix-2.0 >= glib_required_version
                             gtk+-3.0 >= gtk_required_version
                             gtksourceview-3.0 >= gtksourceview_required_version
                             gspell-1 >= gspell_required_version
                             enchant >= enchant_required_version
                             json-glib-1.0 >= json_glib_required_version
                             libpeas-1.0 >= peas_required_version
                             libxml-2.0 >= libxml_required_version
                             pangoft2 >= pangoft2_required_version])
PKG_CHECK_MODULES(MI2,      [gio-2.0 >= glib_required_version])
PKG_CHECK_MODULES(NAUTILUS, [glib-2.0 >= glib_required_version
                             gtk+-3.0 >= gtk_required_version])
PKG_CHECK_MODULES(PANEL_GTK,[gtk+-3.0 >= gtk_required_version])
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
dnl Check if we have WebKit support
dnl ***********************************************************************
m4_define([webkit_required_version], [2.12.0])
AC_ARG_ENABLE([webkit],
              [AS_HELP_STRING([--enable-webkit=@<:@yes/no/auto@:>@],
                              [Build with support for webkit.])],
              [enable_webkit=$enableval],
              [enable_webkit=auto])
AS_IF([test "$enable_webkit" != no],[
	PKG_CHECK_MODULES(WEBKIT,
	                  [webkit2gtk-4.0 >= webkit_required_version],
	                  [have_webkit=yes],
	                  [have_webkit=no])

	AS_IF([test "$enable_webkit" = "yes" && test "$have_webkit" = "no"],[
	       AC_MSG_ERROR([--enable-webkit requires webkit2gtk-4.0 >= webkit_required_version])
	])

	enable_webkit=$have_webkit
])
AM_CONDITIONAL(ENABLE_WEBKIT, [test "x$enable_webkit" = "xyes"])


dnl ***********************************************************************
dnl Check for Required Python
dnl ***********************************************************************
AS_IF([test "x$have_pygobject" = "xyes"],[
	AM_PATH_PYTHON([3.2.3])
	AS_IF([test -z "${PYTHON}"],[
		enable_python_scripting=no
		AC_MSG_RESULT([Failed to locate python3.])
	],[
		enable_python_scripting=yes
		pyoverridesdir="\$(pyexecdir)/gi/overrides"
		AC_SUBST(pyoverridesdir)
	])
])
AM_CONDITIONAL(ENABLE_PYTHON_SCRIPTING, test x$enable_python_scripting != xno)


dnl ***********************************************************************
dnl Check for supported plugins
dnl ***********************************************************************
m4_include([plugins/autotools/configure.ac])
m4_include([plugins/autotools-templates/configure.ac])
m4_include([plugins/beautifier/configure.ac])
m4_include([plugins/c-pack/configure.ac])
m4_include([plugins/cargo/configure.ac])
m4_include([plugins/clang/configure.ac])
m4_include([plugins/color-picker/configure.ac])
m4_include([plugins/cmake/configure.ac])
m4_include([plugins/command-bar/configure.ac])
m4_include([plugins/comment-code/configure.ac])
m4_include([plugins/create-project/configure.ac])
m4_include([plugins/ctags/configure.ac])
m4_include([plugins/devhelp/configure.ac])
m4_include([plugins/eslint/configure.ac])
m4_include([plugins/file-search/configure.ac])
m4_include([plugins/flatpak/configure.ac])
m4_include([plugins/fpaste/configure.ac])
m4_include([plugins/gcc/configure.ac])
m4_include([plugins/gettext/configure.ac])
m4_include([plugins/git/configure.ac])
m4_include([plugins/gnome-code-assistance/configure.ac])
m4_include([plugins/html-completion/configure.ac])
m4_include([plugins/html-preview/configure.ac])
m4_include([plugins/jedi/configure.ac])
m4_include([plugins/jhbuild/configure.ac])
m4_include([plugins/make/configure.ac])
m4_include([plugins/meson/configure.ac])
m4_include([plugins/meson-templates/configure.ac])
m4_include([plugins/mingw/configure.ac])
m4_include([plugins/mono/configure.ac])
m4_include([plugins/phpize/configure.ac])
m4_include([plugins/project-tree/configure.ac])
m4_include([plugins/python-gi-imports-completion/configure.ac])
m4_include([plugins/python-pack/configure.ac])
m4_include([plugins/quick-highlight/configure.ac])
m4_include([plugins/retab/configure.ac])
m4_include([plugins/rust-langserv/configure.ac])
m4_include([plugins/rustup/configure.ac])
m4_include([plugins/support/configure.ac])
m4_include([plugins/symbol-tree/configure.ac])
m4_include([plugins/sysmon/configure.ac])
m4_include([plugins/sysprof/configure.ac])
m4_include([plugins/todo/configure.ac])
m4_include([plugins/terminal/configure.ac])
m4_include([plugins/vala-pack/configure.ac])
m4_include([plugins/valgrind/configure.ac])
m4_include([plugins/xml-pack/configure.ac])


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
	-Werror=format-security \
	-Werror=format=2 \
	-Wextra \
	-Wformat-nonliteral \
	-Wformat-security \
	-Wmissing-include-dirs \
	-Wnested-externs \
	-Wno-missing-field-initializers \
	-Wno-sign-compare \
	-Wno-strict-aliasing \
	-Wno-uninitialized \
	-Wno-unused-parameter \
	-Wpointer-arith \
	-Wredundant-decls \
	-Wshadow \
	-Wswitch-default \
	-Wswitch-enum \
	-Wundef \
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
dnl GCC/Clang sanitizer support
dnl ***********************************************************************
AC_ARG_WITH([sanitizer],
            [AS_HELP_STRING([--with-sanitizer=@<:@address/undefined/no@:>@],
                            [Use specific GCC/Clang analyzer])],
            [with_sanitizer=$withval],
            [with_sanitizer=no])
AS_IF([test "x$with_sanitizer" != "xno"],[
	AX_CHECK_COMPILE_FLAG([-fsanitize=$withval],
	                      [CFLAGS="$CFLAGS -fsanitize=$withval"
			       CXXFLAGS="$CXXFLAGS -fsanitize=$withval"],
	                      [AC_MSG_ERROR([-fsanitize=$withval is not supported])])
])


dnl ***********************************************************************
dnl Setup common cflags and ldflags for plugins
dnl ***********************************************************************
PLUGIN_CFLAGS="$PLUGIN_CFLAGS -I\$(top_builddir)/contrib/egg"
PLUGIN_CFLAGS="$PLUGIN_CFLAGS -I\$(top_builddir)/contrib/pnl"
PLUGIN_CFLAGS="$PLUGIN_CFLAGS -I\$(top_builddir)/libide"
PLUGIN_CFLAGS="$PLUGIN_CFLAGS -I\$(top_srcdir)/contrib/egg"
PLUGIN_CFLAGS="$PLUGIN_CFLAGS -I\$(top_srcdir)/contrib/gd"
PLUGIN_CFLAGS="$PLUGIN_CFLAGS -I\$(top_srcdir)/contrib/gstyle"
PLUGIN_CFLAGS="$PLUGIN_CFLAGS -I\$(top_srcdir)/contrib/jsonrpc-glib"
PLUGIN_CFLAGS="$PLUGIN_CFLAGS -I\$(top_srcdir)/contrib/nautilus"
PLUGIN_CFLAGS="$PLUGIN_CFLAGS -I\$(top_srcdir)/contrib/pnl"
PLUGIN_CFLAGS="$PLUGIN_CFLAGS -I\$(top_srcdir)/contrib/rg"
PLUGIN_CFLAGS="$PLUGIN_CFLAGS -I\$(top_srcdir)/contrib/search"
PLUGIN_CFLAGS="$PLUGIN_CFLAGS -I\$(top_srcdir)/contrib/xml"
PLUGIN_CFLAGS="$PLUGIN_CFLAGS -I\$(top_srcdir)/libide"
PLUGIN_CFLAGS="$PLUGIN_CFLAGS $LIBIDE_CFLAGS"
PLUGIN_CFLAGS="$PLUGIN_CFLAGS $DEBUG_CFLAGS"
PLUGIN_CFLAGS="$PLUGIN_CFLAGS $OPTIMIZE_CFLAGS"
AC_SUBST(PLUGIN_CFLAGS)

PLUGIN_LDFLAGS="$PLUGIN_LDFLAGS $OPTIMIZE_LDFLAGS"
PLUGIN_LDFLAGS="$PLUGIN_LDFLAGS -avoid-version"
PLUGIN_LDFLAGS="$PLUGIN_LDFLAGS -export-symbols-regex peas_register_types"
PLUGIN_LDFLAGS="$PLUGIN_LDFLAGS -module"
AC_SUBST(PLUGIN_LDFLAGS)

PLUGIN_VALAFLAGS="$PLUGIN_VALAFLAGS --target-glib=2.44"
PLUGIN_VALAFLAGS="$PLUGIN_VALAFLAGS --thread"
PLUGIN_VALAFLAGS="$PLUGIN_VALAFLAGS --vapidir \$(top_builddir)/libide"
PLUGIN_VALAFLAGS="$PLUGIN_VALAFLAGS --vapidir \$(top_builddir)/contrib/egg"
PLUGIN_VALAFLAGS="$PLUGIN_VALAFLAGS --vapidir \$(top_builddir)/contrib/gstyle"
PLUGIN_VALAFLAGS="$PLUGIN_VALAFLAGS --vapidir \$(top_builddir)/contrib/jsonrpc-glib"
PLUGIN_VALAFLAGS="$PLUGIN_VALAFLAGS --vapidir \$(top_builddir)/contrib/pnl"
PLUGIN_VALAFLAGS="$PLUGIN_VALAFLAGS --vapidir \$(top_builddir)/contrib/tmpl"
PLUGIN_VALAFLAGS="$PLUGIN_VALAFLAGS --pkg libide-1.0"
PLUGIN_VALAFLAGS="$PLUGIN_VALAFLAGS --pkg libpeas-1.0"
AC_SUBST(PLUGIN_VALAFLAGS)


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
	contrib/gstyle/Makefile
	contrib/gstyle/tests/Makefile
	contrib/jsonrpc-glib/Makefile
	contrib/jsonrpc-glib/jsonrpc-version.h
	contrib/libeditorconfig/Makefile
	contrib/mi2/Makefile
	contrib/nautilus/Makefile
	contrib/pnl/Makefile
	contrib/pnl/pnl-version.h
	contrib/rg/Makefile
	contrib/search/Makefile
	contrib/tmpl/Makefile
	contrib/tmpl/tmpl-debug.h
	contrib/xml/Makefile

	libide/Makefile
	libide/ide-debug.h

	plugins/Makefile

	data/Makefile
	data/fonts/Makefile
	data/gsettings/Makefile
	data/icons/Makefile
	data/icons/hicolor/Makefile
	data/libide-1.0.pc
	data/style-schemes/Makefile
	data/template-glib-1.0.pc
	data/org.gnome.Builder.desktop.in
	data/gsettings/org.gnome.builder.code-insight.gschema.xml

	doc/Makefile
	doc/examples/Makefile

	po/Makefile.in

	src/Makefile

	tests/Makefile

	tools/Makefile
])
AC_OUTPUT


echo ""
echo " ${PACKAGE} - ${VERSION} (${channel})"
echo ""
echo " Options"
echo ""
echo "  Building from Git .................... : ${building_from_git}"
echo "  Prefix ............................... : ${prefix}"
echo "  Libdir ............................... : ${libdir}"
echo "  Optimized Build ...................... : ${enable_optimizations}"
echo "  rdtscp ............................... : ${enable_rdtscp}"
echo "  WebKit ............................... : ${enable_webkit}"
echo ""
echo " Development Options"
echo ""
echo "  Enable Debug ......................... : ${enable_debug}"
echo "  Enable Tracing ....................... : ${enable_tracing}"
echo "  Enable Profiling (-pg)................ : ${enable_profiling}"
echo "  Build Test Suite ..................... : ${enable_tests}"
echo "  Sanitizer ............................ : ${with_sanitizer}"
echo ""
echo " Plugins"
echo ""
echo "  Autotools ............................ : ${enable_autotools_plugin}"
echo "  Beautifier  .......................... : ${enable_beautifier_plugin}"
echo "  C Language Pack ...................... : ${enable_c_pack_plugin}"
echo "  Cargo ................................ : ${enable_cargo_plugin}"
echo "  Clang ................................ : ${enable_clang_plugin}"
echo "  CMake ................................ : ${enable_cmake_plugin}"
echo "  Color picker.......................... : ${enable_color_picker_plugin}"
echo "  Command Bar .......................... : ${enable_command_bar_plugin}"
echo "  Comment Code.......................... : ${enable_comment_code_plugin}"
echo "  Ctags ................................ : ${enable_ctags_plugin}"
echo "  Devhelp .............................. : ${enable_devhelp_plugin}"
echo "  ESLint ............................... : ${enable_eslint_plugin}"
echo "  Editorconfig ......................... : ${enable_editorconfig}"
echo "  Flatpak .............................. : ${enable_flatpak_plugin}"
echo "  Fpaste.org ........................... : ${enable_fpaste_plugin}"
echo "  GCC .................................. : ${enable_gcc_plugin}"
echo "  Gettext .............................. : ${enable_gettext_plugin}"
echo "  Git Version Control .................. : ${enable_git_plugin}"
echo "  Global File Search ................... : ${enable_file_search_plugin}"
echo "  GNOME Code Assistance ................ : ${enable_gnome_code_assistance_plugin}"
echo "  HTML Autocompletion .................. : ${enable_html_completion_plugin}"
echo "  HTML and Markdown Preview ............ : ${enable_html_preview_plugin}"
echo "  JHBuild .............................. : ${enable_jhbuild_plugin}"
echo "  Make ................................. : ${enable_make_plugin}"
echo "  Meson ................................ : ${enable_meson_plugin}"
echo "  MinGW ................................ : ${enable_mingw_plugin}"
echo "  Mono ................................. : ${enable_mono_plugin}"
echo "  PHPize ............................... : ${enable_phpize_plugin}"
echo "  Project Creation ..................... : ${enable_create_project_plugin}"
echo "  Project Tree ......................... : ${enable_project_tree_plugin}"
echo "  Python GObject Introspection ......... : ${enable_python_gi_imports_completion_plugin}"
echo "  Python Jedi Autocompletion ........... : ${enable_jedi_plugin}"
echo "  Python Language Pack ................. : ${enable_python_pack_plugin}"
echo "  Quick Highlight ...................... : ${enable_quick_highlight_plugin}"
echo "  Retab................................. : ${enable_retab_plugin}"
echo "  Rust Language Server ................. : ${enable_rust_langserv_plugin}"
echo "  RustUp ............................... : ${enable_rustup_plugin}"
echo "  Support .............................. : ${enable_support_plugin}"
echo "  System Monitor ....................... : ${enable_sysmon_plugin}"
echo "  Sysprof System Profiler .............. : ${enable_sysprof_plugin}"
echo "  Symbol Tree .......................... : ${enable_symbol_tree_plugin}"
echo "  Todo ................................. : ${enable_todo_plugin}"
echo "  Terminal ............................. : ${enable_terminal_plugin}"
echo "  Vala Language Pack ................... : ${enable_vala_pack_plugin}"
echo "  Valgrind ............................. : ${enable_valgrind_plugin}"
echo "  XML Language Pack .................... : ${enable_xml_pack_plugin}"
echo ""
echo " Templates"
echo ""
echo "  Autotools ............................ : ${enable_autotools_templates}"
echo "  Meson ................................ : ${enable_meson_templates}"
echo ""

