CLEANFILES =
DISTCLEANFILES =
EXTRA_DIST =
BUILT_SOURCES =

pkglibdir = $(libdir)/gnome-builder
pkglib_LTLIBRARIES = libegg-private.la

headersdir = $(includedir)/gnome-builder-@VERSION@/egg
headers_DATA = \
	egg-animation.h \
	egg-binding-group.h \
	egg-box.h \
	egg-centering-bin.h \
	egg-column-layout.h \
	egg-counter.h \
	egg-date-time.h \
	egg-empty-state.h \
	egg-entry-box.h \
	egg-file-chooser-entry.h \
	egg-frame-source.h \
	egg-heap.h \
	egg-list-box.h \
	egg-menu-manager.h \
	egg-pill-box.h \
	egg-priority-box.h \
	egg-private.h \
	egg-radio-box.h \
	egg-scrolled-window.h \
	egg-search-bar.h \
	egg-settings-flag-action.h \
	egg-settings-sandwich.h \
	egg-signal-group.h \
	egg-simple-label.h \
	egg-simple-popover.h \
	egg-slider.h \
	egg-state-machine-buildable.h \
	egg-state-machine.h \
	egg-task-cache.h \
	egg-three-grid.h \
	egg-widget-action-group.h \
	$(NULL)

libegg_private_la_SOURCES = \
	$(headers_DATA) \
	egg-animation.c \
	egg-binding-group.c \
	egg-box.c \
	egg-centering-bin.c \
	egg-column-layout.c \
	egg-counter.c \
	egg-date-time.c \
	egg-empty-state.c \
	egg-entry-box.c \
	egg-file-chooser-entry.c \
	egg-frame-source.c \
	egg-heap.c \
	egg-list-box.c \
	egg-menu-manager.c \
	egg-pill-box.c \
	egg-priority-box.c \
	egg-radio-box.c \
	egg-scrolled-window.c \
	egg-search-bar.c \
	egg-settings-flag-action.c \
	egg-settings-sandwich.c \
	egg-signal-group.c \
	egg-simple-label.c \
	egg-simple-popover.c \
	egg-slider.c \
	egg-state-machine-buildable.c \
	egg-state-machine.c \
	egg-task-cache.c \
	egg-three-grid.c \
	egg-widget-action-group.c \
	$(NULL)

nodist_libegg_private_la_SOURCES = \
	egg-resources.c \
	egg-resources.h \
	$(NULL)

glib_resources_c = egg-resources.c
glib_resources_h = egg-resources.h
glib_resources_xml = egg.gresource.xml
glib_resources_namespace = egg
include $(top_srcdir)/build/autotools/Makefile.am.gresources

libegg_private_la_CFLAGS = \
	$(EGG_CFLAGS) \
	$(NULL)

libegg_private_la_LIBADD = \
	$(EGG_LIBS) \
	$(SHM_LIB) \
	$(LIBM) \
	$(NULL)


if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

Egg-1.0.gir: libegg-private.la
Egg_1_0_gir_INCLUDES = Gio-2.0 Gdk-3.0 Gtk-3.0
Egg_1_0_gir_CFLAGS = $(libegg_private_la_CFLAGS)
Egg_1_0_gir_LIBS = libegg-private.la
Egg_1_0_gir_FILES = $(libegg_private_la_SOURCES)
Egg_1_0_gir_SCANNERFLAGS = \
	--c-include="egg-private.h" \
	-n Egg \
	$(NULL)
INTROSPECTION_GIRS += Egg-1.0.gir

girdir = $(datadir)/gnome-builder/gir-1.0
dist_gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(pkglibdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(dist_gir_DATA) $(typelib_DATA)
endif


if ENABLE_VAPIGEN
-include $(VAPIGEN_MAKEFILE)

egg-private.vapi: Egg-1.0.gir

VAPIGEN_VAPIS = egg-private.vapi

egg_private_vapi_DEPS = gio-2.0 gtk+-3.0
egg_private_vapi_METADATADIRS = $(srcdir)
egg_private_vapi_FILES = Egg-1.0.gir

egg-private.deps: Makefile
	$(AM_V_GEN) echo $(libegg_private_vapi_DEPS) | tr ' ' '\n' > $@

vapidir = $(datadir)/gnome-builder/vapi
vapi_DATA = $(VAPIGEN_VAPIS) $(VAPIGEN_VAPIS:.vapi=.deps)

EXTRA_DIST += egg-private.deps

DISTCLEANFILES += $(vapi_DATA)
endif


-include $(top_srcdir)/git.mk
