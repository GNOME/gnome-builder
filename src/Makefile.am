bin_PROGRAMS = gnome-builder

gnome_builder_SOURCES = main.c

gnome_builder_CFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""${datadir}/locale"\" \
	$(LIBIDE_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(OPTIMIZE_CFLAGS) \
	$(TERMINAL_CFLAGS) \
	-I$(top_builddir)/data/icons/hicolor \
	-I$(top_builddir)/libide \
	-I$(top_srcdir)/libide \
	$(NULL)

gnome_builder_LDADD = \
	$(LIBIDE_LIBS) \
	$(top_builddir)/contrib/egg/libegg-private.la \
	$(top_builddir)/data/icons/hicolor/libicons.la \
	$(top_builddir)/libide/libide-1.0.la \
	$(NULL)

gnome_builder_LDFLAGS = -export-dynamic


-include $(top_srcdir)/git.mk
