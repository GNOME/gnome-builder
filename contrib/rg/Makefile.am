pkglibdir = $(libdir)/gnome-builder
pkglib_LTLIBRARIES = librg.la

librg_la_SOURCES = \
	realtime-graphs.h \
	rg-column.c \
	rg-column.h \
	rg-column-private.h \
	rg-cpu-graph.c \
	rg-cpu-graph.h \
	rg-cpu-table.c \
	rg-cpu-table.h \
	rg-graph.c \
	rg-graph.h \
	rg-line-renderer.c \
	rg-line-renderer.h \
	rg-renderer.c \
	rg-renderer.h \
	rg-ring.c \
	rg-ring.h \
	rg-table.c \
	rg-table.h \
	$(NULL)

librg_la_CFLAGS = \
	-I$(top_srcdir)/contrib/egg \
	$(DEBUG_CFLAGS) \
	$(OPTIMIZE_CFLAGS) \
	$(RG_CFLAGS) \
	$(NULL)

librg_la_LIBADD = \
	$(RG_LIBS) \
	$(top_builddir)/contrib/egg/libegg-private.la \
	$(NULL)

librg_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	$(NULL)

-include $(top_srcdir)/git.mk
