AC_PREREQ(2.60)

m4_define([project_name],[gnome-builder])
m4_define([project_major_version],[0])
m4_define([project_minor_version],[1])
m4_define([project_micro_version],[0])
m4_define([project_interface_age],[0])
m4_define([project_bugreport_url],
	  [https://bugzilla.gnome.org/enter_bug.cgi?product=gnome-builder])

# Initialize autoconf and things that go with it.
m4_include([build/autotools/autoconf.d/pre-ac.m4])
AC_INIT([project_name],[project_version],[project_bugreport_url])
AC_CONFIG_MACRO_DIRS([build/autotools/m4])
AC_CONFIG_AUX_DIR([build/autotools])
AC_SUBST(ACLOCAL_AMFLAGS, "-I build/autotools")
AC_CONFIG_HEADERS([config.h])
m4_include([build/autotools/autoconf.d/post-ac.m4])

# Initlize libtool and things that go with it.
m4_include([build/autotools/autoconf.d/pre-lt.m4])
m4_include([build/autotools/autoconf.d/setup_libtool.m4])
LT_INIT
m4_include([build/autotools/autoconf.d/post-lt.m4])

# Initialize automake and things that go with it.
# We would put AM initialization in a file but autoreconf requires it.
m4_include([build/autotools/autoconf.d/pre-am.m4])
AM_INIT_AUTOMAKE([foreign subdir-objects tar-ustar no-dist-gzip dist-xz])
AM_MAINTAINER_MODE([enable])
m4_include([build/autotools/autoconf.d/post-am.m4])

# If we are using gtk-doc, enable it.
# We would put this in a file but gtkdocize requires it.
m4_ifdef([GTK_DOC_CHECK],[
GTK_DOC_CHECK
])

MAJOR_VERSION=project_major_version
MINOR_VERSION=project_minor_version
MICRO_VERSION=project_micro_version

AC_SUBST(MAJOR_VERSION)
AC_SUBST(MINOR_VERSION)
AC_SUBST(MICRO_VERSION)

# Define our known config files.
AC_CONFIG_FILES([
	Makefile
	src/util/gb-version.h
	data/icons/Makefile
	data/org.gnome.builder.editor.gschema.xml
	data/org.gnome.builder.editor.language.gschema.xml
	data/org.gnome.builder.editor.vim.gschema.xml
])

# Last change to configure things and write configuration files.
m4_include([build/autotools/autoconf.d/pre-output.m4])
AC_OUTPUT
m4_include([build/autotools/autoconf.d/post-output.m4])

# Print configuration information.
m4_include([build/autotools/autoconf.d/print_config.m4])
