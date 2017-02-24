CLEANFILES =
DISTCLEANFILES =
EXTRA_DIST =
BUILT_SOURCES =

SUBDIRS = . tests

pkglibdir = $(libdir)/gnome-builder
pkglib_LTLIBRARIES = libgstyle-private.la

#gettextitsdir = $(datadir)/gettext/its
EXTRA_DIST +=                                       \
	data/its/gstyle.its                         \
	data/its/gstyle.loc

headersdir = $(includedir)/gnome-builder-@VERSION@/gstyle
headers_DATA =                                      \
	gstyle-animation.h                          \
	gstyle-cielab.h                             \
	gstyle-color.h                              \
	gstyle-color-component.h                    \
	gstyle-color-convert.h                      \
	gstyle-color-filter.h                       \
	gstyle-color-item.h                         \
	gstyle-color-panel.h                        \
	gstyle-color-panel-private.h                \
	gstyle-color-panel-actions.h                \
	gstyle-color-plane.h                        \
	gstyle-color-predefined.h                   \
	gstyle-color-scale.h                        \
	gstyle-color-widget.h                       \
	gstyle-color-widget-actions.h               \
	gstyle-colorlexer.h                         \
	gstyle-css-provider.h                       \
	gstyle-eyedropper.h                         \
	gstyle-hsv.h                                \
	gstyle-palette.h                            \
	gstyle-palette-widget.h                     \
	gstyle-private.h                            \
	gstyle-rename-popover.h                     \
	gstyle-revealer.h                           \
	gstyle-slidein.h                            \
	gstyle-types.h                              \
	gstyle-utils.h                              \
	gstyle-xyz.h                                \
	$(NULL)

libgstyle_private_la_SOURCES =                      \
	$(headers_DATA)                             \
	gstyle-animation.c                          \
	gstyle-cielab.c                             \
	gstyle-color.c                              \
	gstyle-color-component.c                    \
	gstyle-color-convert.c                      \
	gstyle-color-filter.c                       \
	gstyle-color-item.c                         \
	gstyle-color-panel.c                        \
	gstyle-color-panel-actions.c                \
	gstyle-color-plane.c                        \
	gstyle-color-scale.c                        \
	gstyle-color-widget.c                       \
	gstyle-color-widget-actions.c               \
	gstyle-colorlexer.c                         \
	gstyle-css-provider.c                       \
	gstyle-eyedropper.c                         \
	gstyle-hsv.c                                \
	gstyle-palette.c                            \
	gstyle-palette-widget.c                     \
	gstyle-rename-popover.c                     \
	gstyle-revealer.c                           \
	gstyle-slidein.c                            \
	gstyle-utils.c                              \
	gstyle-xyz.c                                \
	$(NULL)

libgstyle_private_la_CFLAGS =                       \
	$(GSTYLE_CFLAGS)                            \
	$(XML_CFLAGS)                               \
	$(DEBUG_CFLAGS)                             \
	$(OPTIMIZE_CFLAGS)                          \
	-I$(top_srcdir)/contrib/search              \
	-I$(top_builddir)/contrib/search            \
	$(NULL)

libgstyle_private_la_LIBADD =                       \
	$(GSTYLE_LIBS)                              \
	$(XML_LIBS)                                 \
	$(LIBM)                                     \
	$(top_builddir)/contrib/search/libsearch.la \
	$(NULL)

libgstyle_private_la_LDFLAGS =                      \
	$(OPTIMIZE_LDFLAGS)                         \
	--no-undefined                              \
	$(NULL)

nodist_libgstyle_private_la_SOURCES =               \
	gstyle-resources.c                          \
	gstyle-resources.h                          \
	$(NULL)

glib_resources_h = gstyle-resources.h
glib_resources_c = gstyle-resources.c
glib_resources_xml = gstyle.gresource.xml
glib_resources_namespace = gstyle
include $(top_srcdir)/build/autotools/Makefile.am.gresources


if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

introspection_sources =                             \
	gstyle-cielab.h                             \
	gstyle-cielab.c                             \
	gstyle-color.h                              \
	gstyle-color.c                              \
	gstyle-color-component.h                    \
	gstyle-color-component.c                    \
	gstyle-color-convert.h                      \
	gstyle-color-convert.c                      \
	gstyle-color-filter.h                       \
	gstyle-color-filter.c                       \
	gstyle-color-item.h                         \
	gstyle-color-item.c                         \
	gstyle-color-panel.h                        \
	gstyle-color-panel.c                        \
	gstyle-color-plane.h                        \
	gstyle-color-plane.c                        \
	gstyle-color-scale.h                        \
	gstyle-color-scale.c                        \
	gstyle-color-widget.h                       \
	gstyle-color-widget.c                       \
	gstyle-eyedropper.h                         \
	gstyle-eyedropper.c                         \
	gstyle-hsv.h                                \
	gstyle-hsv.c                                \
	gstyle-palette.h                            \
	gstyle-palette.c                            \
	gstyle-palette-widget.h                     \
	gstyle-palette-widget.c                     \
	gstyle-slidein.h                            \
	gstyle-slidein.c                            \
	gstyle-xyz.h                                \
	gstyle-xyz.c                                \
	$(NULL)

Gstyle-1.0.gir: libgstyle-private.la
Gstyle_1_0_gir_INCLUDES = Gio-2.0 Gdk-3.0 Gtk-3.0 GtkSource-3.0
Gstyle_1_0_gir_CFLAGS = $(libgstyle_private_la_CFLAGS)
Gstyle_1_0_gir_LIBS = libgstyle-private.la
Gstyle_1_0_gir_FILES = $(introspection_sources)
Gstyle_1_0_gir_SCANNERFLAGS =                       \
	--c-include="gstyle-private.h"              \
	-n Gstyle                                   \
	--identifier-prefix Gstyle                  \
	--symbol-prefix gstyle                      \
	$(NULL)
INTROSPECTION_GIRS += Gstyle-1.0.gir

girdir = $(datadir)/gnome-builder/gir-1.0
dist_gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(pkglibdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(dist_gir_DATA) $(typelib_DATA)
endif


if ENABLE_VAPIGEN
-include $(VAPIGEN_MAKEFILE)

gstyle-private.vapi: Gstyle-1.0.gir

VAPIGEN_VAPIS = gstyle-private.vapi

gstyle_private_vapi_DEPS = gio-2.0 gtk+-3.0 GtkSource-3.0
gstyle_private_vapi_METADATADIRS = $(srcdir)
gstyle_private_vapi_FILES = Gstyle-1.0.gir

gstyle-private.deps: Makefile
	$(AM_V_GEN) echo $(libgstyle_private_vapi_DEPS) | tr ' ' '\n' > $@

vapidir = $(datadir)/gnome-builder/vapi
vapi_DATA = $(VAPIGEN_VAPIS) $(VAPIGEN_VAPIS:.vapi=.deps)

EXTRA_DIST += gstyle-private.deps

DISTCLEANFILES += $(vapi_DATA)
endif

-include $(top_srcdir)/git.mk
