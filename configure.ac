AC_PREREQ(2.60)

m4_include([build/autotools/project_info.m4])

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
AM_INIT_AUTOMAKE([foreign subdir-objects tar-ustar])
AM_MAINTAINER_MODE([enable])
m4_include([build/autotools/autoconf.d/post-am.m4])

# If we are using gtk-doc, enable it.
# We would put this in a file but gtkdocize requires it.
m4_ifdef([GTK_DOC_CHECK],[
GTK_DOC_CHECK
])

# Define our known config files.
AC_CONFIG_FILES([Makefile])

# Last change to configure things and write configuration files.
m4_include([build/autotools/autoconf.d/pre-output.m4])
AC_OUTPUT
m4_include([build/autotools/autoconf.d/post-output.m4])

# Print configuration information.
m4_include([build/autotools/autoconf.d/print_config.m4])
