if ENABLE_GDB_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libgdb-plugin.la
dist_plugin_DATA = gdb.plugin

libgdb_plugin_la_SOURCES = \
	gbp-gdb-debugger.c \
	gbp-gdb-debugger.h \
	gbp-gdb-plugin.c   \
	$(NULL)

libgdb_plugin_la_CFLAGS =           \
	$(PLUGIN_CFLAGS)            \
	-I$(top_srcdir)/contrib/mi2 \
	$(NULL)

libgdb_plugin_la_LDFLAGS =                         \
	$(PLUGIN_LDFLAGS)                          \
	$(top_builddir)/contrib/mi2/libmi2-glib.la \
	$(NULL)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk
