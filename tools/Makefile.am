tools_PROGRAMS = ide-list-counters
toolsdir = $(libexecdir)/gnome-builder

ide_list_counters_SOURCES = ide-list-counters.c
ide_list_counters_CFLAGS =                            \
	$(EGG_CFLAGS)                                 \
	-I$(top_srcdir)/contrib/egg                   \
	$(NULL)
ide_list_counters_LDADD =                             \
	$(EGG_LIBS)                                   \
	$(top_builddir)/contrib/egg/libegg-private.la \
	$(SHM_LIB)                                    \
	$(NULL)

-include $(top_srcdir)/git.mk
