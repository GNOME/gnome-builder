AC_PREREQ([2.69])


dnl ***********************************************************************
dnl Define Versioning Information
dnl ***********************************************************************
m4_define([major_version],[{{major_version}}])
m4_define([minor_version],[{{minor_version}}])
m4_define([micro_version],[{{micro_version}}])
m4_define([package_version],[major_version.minor_version.micro_version])
m4_define([bug_report_url],[https://bugzilla.gnome.org/enter_bug.cgi?product={{name}}])
m4_define([api_version],[major_version])

AX_IS_RELEASE([micro-version])

dnl ***********************************************************************
dnl Initialize autoconf
dnl ***********************************************************************
AC_INIT([{{name}}],[package_version],[bug_report_url])
AC_CONFIG_HEADERS([config.h])
{{if template == "shared-library"}}
AC_CONFIG_SRCDIR([data/{{name}}.pc.in])
{{else}}
AC_CONFIG_SRCDIR([NEWS])
{{end}}
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_SUBST([ACLOCAL_AMFLAGS], "-I m4")
AC_CANONICAL_HOST


dnl ***********************************************************************
dnl Make version information available to autoconf files
dnl ***********************************************************************
AC_SUBST([MAJOR_VERSION],major_version)
AC_SUBST([MINOR_VERSION],minor_version)
AC_SUBST([MICRO_VERSION],micro_version)
AC_SUBST([API_VERSION],api_version)


dnl ***********************************************************************
dnl Initialize automake
dnl ***********************************************************************
AM_SILENT_RULES([yes])
AM_INIT_AUTOMAKE([1.11 foreign subdir-objects tar-ustar no-dist-gzip dist-xz -Wno-portability])
AM_MAINTAINER_MODE([enable])
AX_GENERATE_CHANGELOG


dnl ***********************************************************************
dnl Add extra debugging with --enable-debug and --enable-compile-warnings
dnl ***********************************************************************
AX_CHECK_ENABLE_DEBUG([no],[]
                      [G_DISABLE_ASSERT G_DISABLE_CHECKS G_DISABLE_CAST_CHECKS])


{{if enable_i18n}}
dnl ***********************************************************************
dnl Internationalization
dnl ***********************************************************************
GETTEXT_PACKAGE=AC_PACKAGE_TARNAME
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [GETTEXT package name])

AM_GNU_GETTEXT_VERSION([0.19.6])
AM_GNU_GETTEXT([external])


{{end}}
dnl ***********************************************************************
dnl Check for required programs
dnl ***********************************************************************
{{if enable_c}}
AC_PROG_CC
{{end}}
{{if enable_cplusplus}}
AC_PROG_CXX
{{end}}
AC_PROG_INSTALL
AC_PROG_SED
AC_PATH_PROG([GLIB_GENMARSHAL],[glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums])
AC_PATH_PROG([GLIB_COMPILE_RESOURCES],[glib-compile-resources])
PKG_PROG_PKG_CONFIG([0.22])
GLIB_GSETTINGS
{{if enable_gobject_introspection}}
GOBJECT_INTROSPECTION_CHECK([1.42.0])
{{end}}
{{if enable_vala}}
AM_PROG_VALAC([0.32])
{{end}}
{{if enable_vapi}}
VAPIGEN_CHECK
{{end}}
{{if enable_c}}


AX_COMPILER_FLAGS
{{end}}


{{if enable_c}}
dnl ***********************************************************************
dnl Ensure C11 is Supported
dnl ***********************************************************************
AX_CHECK_COMPILE_FLAG([-std=gnu11],
                      [CFLAGS="$CFLAGS -std=gnu11"],
                      [AC_MSG_ERROR([C compiler cannot compile GNU C11 code])])


{{end}}
{{if enable_cplusplus}}
dnl ***********************************************************************
dnl Ensure C++11 is Supported
dnl ***********************************************************************
AX_CXX_COMPILE_STDCXX_11([noext], [mandatory])


{{end}}
{{if enable_python}}
dnl ***********************************************************************
dnl Check for required Python
dnl ***********************************************************************
AM_PATH_PYTHON([3.3])


{{end}}
{{if enable_c}}
dnl ***********************************************************************
dnl Check for required packages
dnl ***********************************************************************
PKG_CHECK_MODULES({{PREFIX}}, [{{packages}}])


dnl ***********************************************************************
dnl Initialize Libtool
dnl ***********************************************************************
LT_PREREQ([2.2])
LT_INIT


{{end}}
dnl ***********************************************************************
dnl Process .in Files
dnl ***********************************************************************
AC_CONFIG_FILES([
	Makefile

{{if enable_c}}
	src/Makefile
{{end}}
{{if enable_python}}
	bin/Makefile
	{{name_}}/Makefile
{{end}}
{{if template == "shared-library"}}
{{if language != "vala"}}
	src/{{prefix}}-version.h
{{end}}
{{end}}

	data/Makefile
{{if template == "shared-library"}}
	data/{{name}}-$API_VERSION.pc:data/{{name}}.pc.in
{{end}}

{{if enable_i18n}}
	po/Makefile.in
{{end}}
],[],
[API_VERSION='$API_VERSION'])
AC_OUTPUT

echo ""
echo " ${PACKAGE} - ${VERSION}"
echo ""
echo " Options"
echo ""
echo "  Prefix ............................... : ${prefix}"
echo "  Libdir ............................... : ${libdir}"
echo ""
