CLEANFILES =
DISTCLEANFILES =
EXTRA_DIST =
BUILT_SOURCES =

pkglibdir = $(libdir)/gnome-builder
pkglib_LTLIBRARIES = libpanel-gtk.la

headersdir = $(includedir)/gnome-builder-@VERSION@/pnl
headers_DATA = \
	pnl-animation.h \
	pnl-dock-bin.h \
	pnl-dock-bin-edge.h \
	pnl-dock-item.h \
	pnl-dock-manager.h \
	pnl-dock-overlay.h \
	pnl-dock-paned.h \
	pnl-dock-revealer.h \
	pnl-dock-stack.h \
	pnl-dock-types.h \
	pnl-dock-widget.h \
	pnl-dock-window.h \
	pnl-dock.h \
	pnl-frame-source.h \
	pnl-multi-paned.h \
	pnl-tab-strip.h \
	pnl-tab.h \
	pnl-version.h \
	pnl.h \
	$(NULL)

libpanel_gtk_la_SOURCES = \
	$(headers_DATA) \
	pnl-animation.c \
	pnl-dock-bin-edge.c \
	pnl-dock-bin-edge-private.h \
	pnl-dock-bin.c \
	pnl-dock-item.c \
	pnl-dock-manager.c \
	pnl-dock-overlay-edge-private.h \
	pnl-dock-overlay-edge.c \
	pnl-dock-overlay.c \
	pnl-dock-paned-private.h \
	pnl-dock-paned.c \
	pnl-dock-revealer.c \
	pnl-dock-stack.c \
	pnl-dock-tab-strip.c \
	pnl-dock-tab-strip.h \
	pnl-dock-transient-grab.c \
	pnl-dock-transient-grab.h \
	pnl-dock-widget.c \
	pnl-dock-window.c \
	pnl-dock.c \
	pnl-frame-source.c \
	pnl-multi-paned.c \
	pnl-tab-strip.c \
	pnl-tab.c \
	pnl-util-private.h \
	pnl-util.c \
	$(NULL)

nodist_libpanel_gtk_la_SOURCES = pnl-resources.c pnl-resources.h

libpanel_gtk_la_CFLAGS = \
	-DPNL_COMPILATION \
	$(PANEL_GTK_CFLAGS) \
	$(NULL)

libpanel_gtk_la_LIBADD = \
	$(PANEL_GTK_LIBS) \
	$(NULL)

libpanel_gtk_la_LDFLAGS = \
	--no-undefined \
	$(NULL)


glib_resources_h = pnl-resources.h
glib_resources_c = pnl-resources.c
glib_resources_xml = pnl.gresource.xml
glib_resources_namespace = pnl
include $(top_srcdir)/build/autotools/Makefile.am.gresources


if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

Pnl-1.0.gir: libpanel-gtk.la
Pnl_1_0_gir_INCLUDES = Gio-2.0 Gdk-3.0 Gtk-3.0
Pnl_1_0_gir_CFLAGS = $(libpanel_gtk_la_CFLAGS) -DPNL_COMPILATION
Pnl_1_0_gir_LIBS = libpanel-gtk.la
Pnl_1_0_gir_FILES = $(libpanel_gtk_la_SOURCES)
Pnl_1_0_gir_SCANNERFLAGS = \
	--c-include="pnl.h" \
	-n Pnl \
	$(NULL)
INTROSPECTION_GIRS += Pnl-1.0.gir

girdir = $(datadir)/gnome-builder/gir-1.0
dist_gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(pkglibdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(dist_gir_DATA) $(typelib_DATA)
endif


if ENABLE_VAPIGEN
-include $(VAPIGEN_MAKEFILE)

panel-gtk.vapi: Pnl-1.0.gir

VAPIGEN_VAPIS = panel-gtk.vapi

panel_gtk_vapi_DEPS = gio-2.0 gtk+-3.0
panel_gtk_vapi_METADATADIRS = $(srcdir)
panel_gtk_vapi_FILES = Pnl-1.0.gir

panel-gtk.deps: Makefile
	$(AM_V_GEN) echo $(libpanel_gtk_vapi_DEPS) | tr ' ' '\n' > $@

vapidir = $(datadir)/gnome-builder/vapi
vapi_DATA = $(VAPIGEN_VAPIS) $(VAPIGEN_VAPIS:.vapi=.deps)

EXTRA_DIST += panel-gtk.deps

DISTCLEANFILES += $(vapi_DATA)
endif


-include $(top_srcdir)/git.mk
