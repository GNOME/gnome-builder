bin_PROGRAMS = \
	gnome-builder \
	gnome-builder-worker \
	ide

AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""${datadir}/locale"\" \
	$(LIBIDE_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(OPTIMIZE_CFLAGS) \
	$(TERMINAL_CFLAGS) \
	-I$(top_builddir)/data/icons/hicolor \
	-I$(top_builddir)/libide \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/contrib/pnl \
	-I$(top_builddir)/contrib/pnl \
	$(NULL)

gnome_builder_libs = \
	$(LIBIDE_LIBS) \
	$(top_builddir)/contrib/egg/libegg-private.la \
	$(top_builddir)/contrib/pnl/libpanel-gtk.la \
	$(top_builddir)/data/icons/hicolor/libicons.la \
	$(top_builddir)/libide/libide-1.0.la \
	$(NULL)

AM_LDFLAGS = -export-dynamic

gnome_builder_SOURCES = main.c
gnome_builder_LDADD = $(gnome_builder_libs)

gnome_builder_worker_SOURCES = main.c
gnome_builder_worker_LDADD = $(gnome_builder_libs)

ide_SOURCES = main.c
ide_LDADD = $(gnome_builder_libs)

-include $(top_srcdir)/git.mk
