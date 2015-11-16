CLEANFILES =
DISTCLEANFILES =
EXTRA_DIST =
BUILT_SOURCES =

pkglibdir = $(libdir)/gnome-builder
pkglib_LTLIBRARIES = libegg-private.la

libegg_private_la_SOURCES = \
	egg-animation.c \
	egg-animation.h \
	egg-binding-group.c \
	egg-binding-group.h \
	egg-centering-bin.c \
	egg-centering-bin.h \
	egg-counter.c \
	egg-counter.h \
	egg-date-time.c \
	egg-date-time.h \
	egg-frame-source.c \
	egg-frame-source.h \
	egg-heap.c \
	egg-heap.h \
	egg-pill-box.c \
	egg-pill-box.h \
	egg-private.h \
	egg-scrolled-window.c \
	egg-scrolled-window.h \
	egg-search-bar.c \
	egg-search-bar.h \
	egg-settings-flag-action.c \
	egg-settings-flag-action.h \
	egg-settings-sandwich.c \
	egg-settings-sandwich.h \
	egg-signal-group.c \
	egg-signal-group.h \
	egg-slider.c \
	egg-slider.h \
	egg-state-machine.c \
	egg-state-machine.h \
	egg-state-machine-action.c \
	egg-state-machine-action.h \
	egg-state-machine-buildable.c \
	egg-state-machine-buildable.h \
	egg-task-cache.c \
	egg-task-cache.h \
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
	$(NULL)


if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

introspection_sources = \
	egg-animation.c \
	egg-animation.h \
	egg-binding-group.c \
	egg-binding-group.h \
	egg-search-bar.c \
	egg-search-bar.h \
	egg-settings-flag-action.c \
	egg-settings-flag-action.h \
	egg-settings-sandwich.c \
	egg-settings-sandwich.h \
	egg-signal-group.c \
	egg-signal-group.h \
	egg-state-machine.c \
	egg-state-machine.h \
	egg-task-cache.c \
	egg-task-cache.h \
	$(NULL)

Egg-1.0.gir: libegg-private.la
Egg_1_0_gir_INCLUDES = Gio-2.0 Gdk-3.0 Gtk-3.0
Egg_1_0_gir_CFLAGS = $(libegg_private_la_CFLAGS)
Egg_1_0_gir_LIBS = libegg-private.la
Egg_1_0_gir_FILES = $(introspection_sources)
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
