CLEANFILES =
DISTCLEANFILES =
BUILT_SOURCES =
EXTRA_DIST =

bin_PROGRAMS = gnome-builder

pkglibdir = $(libdir)/gnome-builder
pkglib_LTLIBRARIES = libgnome-builder.la

if ENABLE_PYTHON_SCRIPTING
overridesdir = $(pyoverridesdir)
overrides_PYTHON = Builder.py
endif

# XXX: Keep in sync with gnome-builder.h
libgnome_builder_public_sources = \
	gnome-builder.h \
	app/gb-application.c \
	app/gb-application.h \
	app/gb-application-addin.c \
	app/gb-application-addin.h \
	documents/gb-document.c \
	documents/gb-document.h \
	editor/gb-editor-view.c \
	editor/gb-editor-view.h \
	editor/gb-editor-view-addin.c \
	editor/gb-editor-view-addin.h \
	util/gb-menu-extension.c \
	util/gb-menu-extension.h \
	tree/gb-tree-builder.c \
	tree/gb-tree-builder.h \
	tree/gb-tree.c \
	tree/gb-tree.h \
	tree/gb-tree-node.c \
	tree/gb-tree-node.h \
	tree/gb-tree-types.h \
	views/gb-view.c \
	views/gb-view.h \
	views/gb-view-stack.c \
	views/gb-view-stack.h \
	views/gb-view-stack-split.h \
	views/gb-view-grid.c \
	views/gb-view-grid.h \
	workbench/gb-workbench.c \
	workbench/gb-workbench.h \
	gb-enums.c \
	gb-enums.h \
	$(NULL)

libgnome_builder_la_SOURCES = \
	$(gnome_builder_built_sources) \
	$(libgnome_builder_public_sources) \
	app/gb-application-actions.c \
	app/gb-application-actions.h \
	app/gb-application-credits.h \
	app/gb-application-private.h \
	dialogs/gb-new-project-dialog.c \
	dialogs/gb-new-project-dialog.h \
	dialogs/gb-projects-dialog.c \
	dialogs/gb-projects-dialog.h \
	dialogs/gb-recent-project-row.c \
	dialogs/gb-recent-project-row.h \
	editor/gb-editor-document.c \
	editor/gb-editor-document.h \
	editor/gb-editor-frame-actions.c \
	editor/gb-editor-frame-actions.h \
	editor/gb-editor-frame-private.h \
	editor/gb-editor-frame.c \
	editor/gb-editor-frame.h \
	editor/gb-editor-map-bin.c \
	editor/gb-editor-map-bin.h \
	editor/gb-editor-print-operation.c \
	editor/gb-editor-print-operation.h \
	editor/gb-editor-settings-widget.c \
	editor/gb-editor-settings-widget.h \
	editor/gb-editor-tweak-widget.c \
	editor/gb-editor-tweak-widget.h \
	editor/gb-editor-view-actions.c \
	editor/gb-editor-view-actions.h \
	editor/gb-editor-view-addin-private.h \
	editor/gb-editor-view-private.h \
	greeter/gb-greeter-project-row.c \
	greeter/gb-greeter-project-row.h \
	greeter/gb-greeter-window.c \
	greeter/gb-greeter-window.h \
	keybindings/gb-keybindings.c \
	keybindings/gb-keybindings.h \
	preferences/gb-preferences-page-editor.c \
	preferences/gb-preferences-page-editor.h \
	preferences/gb-preferences-page-insight.c \
	preferences/gb-preferences-page-insight.h \
	preferences/gb-preferences-page-git.c \
	preferences/gb-preferences-page-git.h \
	preferences/gb-preferences-page-keybindings.c \
	preferences/gb-preferences-page-keybindings.h \
	preferences/gb-preferences-page-language.c \
	preferences/gb-preferences-page-language.h \
	preferences/gb-preferences-page-plugins.c \
	preferences/gb-preferences-page-plugins.h \
	preferences/gb-preferences-page-theme.c \
	preferences/gb-preferences-page-theme.h \
	preferences/gb-preferences-page.c \
	preferences/gb-preferences-page.h \
	preferences/gb-preferences-switch.c \
	preferences/gb-preferences-switch.h \
	preferences/gb-preferences-window.c \
	preferences/gb-preferences-window.h \
	project-tree/gb-new-file-popover.c \
	project-tree/gb-new-file-popover.h \
	project-tree/gb-project-file.c \
	project-tree/gb-project-file.h \
	project-tree/gb-project-tree-actions.c \
	project-tree/gb-project-tree-actions.h \
	project-tree/gb-project-tree-builder.c \
	project-tree/gb-project-tree-builder.h \
	project-tree/gb-project-tree-private.h \
	project-tree/gb-project-tree.c \
	project-tree/gb-project-tree.h \
	project-tree/gb-rename-file-popover.c \
	project-tree/gb-rename-file-popover.h \
	scrolledwindow/gb-scrolled-window.c \
	scrolledwindow/gb-scrolled-window.h \
	shortcuts/gb-accel-label.c \
	shortcuts/gb-accel-label.h \
	shortcuts/gb-shortcuts-column.c \
	shortcuts/gb-shortcuts-column.h \
	shortcuts/gb-shortcuts-dialog.c \
	shortcuts/gb-shortcuts-dialog.h \
	shortcuts/gb-shortcuts-gesture.c \
	shortcuts/gb-shortcuts-gesture.h \
	shortcuts/gb-shortcuts-group.c \
	shortcuts/gb-shortcuts-group.h \
	shortcuts/gb-shortcuts-page.c \
	shortcuts/gb-shortcuts-page.h \
	shortcuts/gb-shortcuts-shortcut.c \
	shortcuts/gb-shortcuts-shortcut.h \
	shortcuts/gb-shortcuts-view.c \
	shortcuts/gb-shortcuts-view.h \
	shortcuts/gb-shortcuts-window.c \
	shortcuts/gb-shortcuts-window.h \
	search/gb-search-box.c \
	search/gb-search-box.h \
	search/gb-search-display-group.c \
	search/gb-search-display-group.h \
	search/gb-search-display-row.c \
	search/gb-search-display-row.h \
	search/gb-search-display.c \
	search/gb-search-display.h \
	support/gb-support.c \
	support/gb-support.h \
	tree/gb-tree-private.h \
	util/gb-cairo.c \
	util/gb-cairo.h \
	util/gb-dnd.c \
	util/gb-dnd.h \
	util/gb-file-manager.c \
	util/gb-file-manager.h \
	util/gb-gdk.c \
	util/gb-gdk.h \
	util/gb-glib.c \
	util/gb-glib.h \
	util/gb-gtk.c \
	util/gb-gtk.h \
	util/gb-pango.c \
	util/gb-pango.h \
	util/gb-plugins.c \
	util/gb-plugins.h \
	util/gb-rgba.c \
	util/gb-rgba.h \
	util/gb-simple-popover.c \
	util/gb-simple-popover.h \
	util/gb-string.c \
	util/gb-string.h \
	util/gb-widget.c \
	util/gb-widget.h \
	views/gb-view-stack-actions.c \
	views/gb-view-stack-actions.h \
	views/gb-view-stack-private.h \
	workbench/gb-workbench-actions.c \
	workbench/gb-workbench-actions.h \
	workbench/gb-workbench-addin.c \
	workbench/gb-workbench-addin.h \
	workbench/gb-workbench-private.h \
	workbench/gb-workbench-types.h \
	workspace/gb-workspace.c \
	workspace/gb-workspace.h \
	workspace/gb-workspace-pane.c \
	workspace/gb-workspace-pane.h \
	workspace/gb-slider.c \
	workspace/gb-slider.h \
	css/gb-css-provider.c \
	css/gb-css-provider.h \
	util/gb-settings.c \
	util/gb-settings.h \
	$(NULL)

glib_enum_h = gb-enums.h
glib_enum_c = gb-enums.c
glib_enum_headers =  \
	views/gb-view-stack-split.h \
	$(NULL)
include $(top_srcdir)/build/autotools/Makefile.am.enums

nodist_libgnome_builder_la_SOURCES = \
	resources/gb-resources.c \
	resources/gb-resources.h \
	$(NULL)

libgnome_builder_la_LIBADD = \
	$(BUILDER_LIBS) \
	$(OPTIMIZE_LDFLAGS) \
	$(TERMINAL_LIBS) \
	$(top_builddir)/data/icons/hicolor/libicons.la \
	$(top_builddir)/libide/libide-1.0.la \
	$(top_builddir)/contrib/egg/libegg-private.la \
	$(top_builddir)/contrib/gd/libgd.la \
	$(top_builddir)/contrib/gedit/libgedit.la \
	$(top_builddir)/contrib/nautilus/libnautilus.la \
	$(top_builddir)/contrib/rg/librg.la \
	-lm

libgnome_builder_la_LDFLAGS = \
	-export-symbols-regex '^gb_.*' \
	-export-dynamic \
	$(NULL)

libgnome_builder_la_CFLAGS = \
	-DPACKAGE_DATADIR="\"${datadir}\"" \
	-DPACKAGE_LOCALE_DIR=\""${datadir}/locale"\" \
	-DPACKAGE_LIBDIR=\""${libdir}"\" \
	-DBUILDDIR=\""${abs_top_builddir}"\" \
	$(BUILDER_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(OPTIMIZE_CFLAGS) \
	$(TERMINAL_CFLAGS) \
	-I$(builddir)/resources \
	-I$(builddir)/util \
	-I$(srcdir)/app \
	-I$(srcdir)/css \
	-I$(srcdir)/dialogs \
	-I$(srcdir)/documents \
	-I$(srcdir)/editor \
	-I$(srcdir)/greeter \
	-I$(srcdir)/keybindings \
	-I$(srcdir)/preferences \
	-I$(srcdir)/project-tree \
	-I$(srcdir)/resources \
	-I$(srcdir)/scrolledwindow \
	-I$(srcdir)/search \
	-I$(srcdir)/shortcuts \
	-I$(srcdir)/support \
	-I$(srcdir)/tree \
	-I$(srcdir)/util \
	-I$(srcdir)/views \
	-I$(srcdir)/workbench \
	-I$(srcdir)/workspace \
	-I$(top_builddir)/data/icons/hicolor \
	-I$(top_builddir)/libide \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/contrib/egg \
	-I$(top_srcdir)/contrib/gd \
	-I$(top_srcdir)/contrib/gedit \
	-I$(top_srcdir)/contrib/nautilus \
	-I$(top_srcdir)/contrib/rg \
	$(NULL)


# FIXME: Move this code to terminal plugin
if ENABLE_TERMINAL_PLUGIN
libgnome_builder_la_CFLAGS += -DHAVE_VTE
endif


gnome_builder_SOURCES = main.c
gnome_builder_CFLAGS = $(libgnome_builder_la_CFLAGS)
gnome_builder_LDADD = \
	$(BUILDER_LIBS) \
	$(top_builddir)/contrib/egg/libegg-private.la \
	$(top_builddir)/data/icons/hicolor/libicons.la \
	$(top_builddir)/libide/libide-1.0.la \
	$(NULL)
gnome_builder_LDFLAGS = \
	-export-dynamic \
	$(NULL)


glib_resources_c = resources/gb-resources.c
glib_resources_h = resources/gb-resources.h
glib_resources_xml = resources/gnome-builder.gresource.xml
glib_resources_namespace = gb
include $(top_srcdir)/build/autotools/Makefile.am.gresources


if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(top_builddir)/libide

introspection_sources = \
	$(libgnome_builder_public_sources) \
	$(NULL)

Builder-1.0.gir: libgnome-builder.la
Builder_1_0_gir_INCLUDES = Gio-2.0 GtkSource-3.0 Peas-1.0
Builder_1_0_gir_CFLAGS = $(libgnome_builder_la_CFLAGS)
Builder_1_0_gir_LIBS = \
	libgnome-builder.la \
	$(top_builddir)/contrib/egg/libegg-private.la \
	$(top_builddir)/libide/libide-1.0.la
Builder_1_0_gir_FILES = $(introspection_sources)
Builder_1_0_gir_SCANNERFLAGS = \
	--include-uninstalled=$(top_builddir)/libide/Ide-1.0.gir \
	--c-include="gnome-builder.h" \
	-n Builder \
	--identifier-prefix=Gb \
	--symbol-prefix=gb \
	--nsversion 1.0 \
	$(NULL)
INTROSPECTION_GIRS += Builder-1.0.gir

girdir = $(datadir)/gnome-builder/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(pkglibdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif


if ENABLE_VAPIGEN
-include $(VAPIGEN_MAKEFILE)

gnome-builder-1.0.vapi: Builder-1.0.gir

VAPIGEN_VAPIS = gnome-builder-1.0.vapi

gnome_builder_1_0_vapi_DEPS = libide-1.0 gio-2.0 gtk+-3.0 gtksourceview-3.0
gnome_builder_1_0_vapi_VAPIDIRS = $(top_builddir)/libide
gnome_builder_1_0_vapi_METADATADIRS = $(srcdir)
gnome_builder_1_0_vapi_FILES = Builder-1.0.gir
gnome_builder_1_0_vapi_GIRDIRS = \
	$(top_builddir)/libide \
	$(top_srcdir)/libide

gnome-builder-1.0.deps: Makefile
	$(AM_V_GEN) echo $(gnome_builder_1_0_vapi_DEPS) | tr ' ' '\n' > $@

vapidir = $(datadir)/gnome-builder/vapi
vapi_DATA = $(VAPIGEN_VAPIS) $(VAPIGEN_VAPIS:.vapi=.deps)

EXTRA_DIST += gnome-builder-1.0.deps

DISTCLEANFILES += $(vapi_DATA)
endif


-include $(top_srcdir)/git.mk
