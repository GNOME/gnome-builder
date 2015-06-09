DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST =

noinst_LTLIBRARIES = libterminal.la

libterminal_la_SOURCES = \
	gb-terminal.c \
	gb-terminal.h \
	gb-terminal-addin.c \
	gb-terminal-addin.h \
	gb-terminal-application-addin.c \
	gb-terminal-application-addin.h \
	gb-terminal-plugin.c \
	$(NULL)

nodist_libterminal_la_SOURCES = \
	gb-terminal-resources.c \
	gb-terminal-resources.h \
	$(NULL)

libterminal_la_CFLAGS = \
	$(BUILDER_CFLAGS) \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/app \
	-I$(top_srcdir)/src/documents \
	-I$(top_srcdir)/src/util \
	-I$(top_srcdir)/src/views \
	-I$(top_srcdir)/src/workbench \
	-I$(top_srcdir)/src/workspace \
	-I$(top_srcdir)/contrib/egg \
	$(NULL)

libterminal_la_LIBADD = \
	$(BUILDER_LIBS) \
	$(NULL)

libterminal_la_LDFLAGS = -module

glib_resources_c = gb-terminal-resources.c
glib_resources_h = gb-terminal-resources.h
glib_resources_xml = gb-terminal.gresource.xml
glib_resources_namespace = gb_terminal
include $(top_srcdir)/build/autotools/Makefile.am.gresources


-include $(top_srcdir)/git.mk
