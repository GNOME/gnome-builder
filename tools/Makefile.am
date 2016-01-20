tools_PROGRAMS =

tools_cflags = \
	-I$(top_srcdir)/libide \
	-I$(top_builddir)/libide \
	-I$(top_srcdir)/src/util \
	$(LIBIDE_CFLAGS) \
	$(NULL)

tools_libs = \
	$(LIBIDE_LIBS) \
	$(top_builddir)/libide/libide-1.0.la \
	$(top_builddir)/contrib/egg/libegg-private.la \
	$(NULL)

toolsdir = $(libexecdir)/gnome-builder

tools_PROGRAMS += ide-list-counters
ide_list_counters_SOURCES = ide-list-counters.c
ide_list_counters_CFLAGS = \
	$(EGG_CFLAGS) \
	-I$(top_srcdir)/contrib/egg \
	$(NULL)
ide_list_counters_LDADD = \
	$(EGG_LIBS) \
	$(top_builddir)/contrib/egg/libegg-private.la \
	$(SHM_LIB) \
	$(NULL)

-include $(top_srcdir)/git.mk
