CLEANFILES =
DISTCLEANFILES =
BUILT_SOURCES =
EXTRA_DIST =

bin_PROGRAMS = gnome-builder

noinst_LTLIBRARIES = libgnome-builder.la

libgnome_builder_la_SOURCES = \
	$(gnome_builder_built_sources) \
	app/gb-application-actions.c \
	app/gb-application-actions.h \
	app/gb-application-addin.c \
	app/gb-application-addin.h \
	app/gb-application-credits.h \
	app/gb-application-private.h \
	app/gb-application.c \
	app/gb-application.h \
	dialogs/gb-new-project-dialog.c \
	dialogs/gb-new-project-dialog.h \
	dialogs/gb-projects-dialog.c \
	dialogs/gb-projects-dialog.h \
	dialogs/gb-recent-project-row.c \
	dialogs/gb-recent-project-row.h \
	documents/gb-document.c \
	documents/gb-document.h \
	editor/gb-editor-document.c \
	editor/gb-editor-document.h \
	editor/gb-editor-frame-actions.c \
	editor/gb-editor-frame-actions.h \
	editor/gb-editor-frame-private.h \
	editor/gb-editor-frame.c \
	editor/gb-editor-frame.h \
	editor/gb-editor-map-bin.c \
	editor/gb-editor-map-bin.h \
	editor/gb-editor-settings-widget.c \
	editor/gb-editor-settings-widget.h \
	editor/gb-editor-tweak-widget.c \
	editor/gb-editor-tweak-widget.h \
	editor/gb-editor-view-actions.c \
	editor/gb-editor-view-actions.h \
	editor/gb-editor-view-addin.c \
	editor/gb-editor-view-addin.h \
	editor/gb-editor-view-private.h \
	editor/gb-editor-view.c \
	editor/gb-editor-view.h \
	greeter/gb-greeter-pill-box.c \
	greeter/gb-greeter-pill-box.h \
	greeter/gb-greeter-project-row.c \
	greeter/gb-greeter-project-row.h \
	greeter/gb-greeter-window.c \
	greeter/gb-greeter-window.h \
	html/gb-html-document.c \
	html/gb-html-document.h \
	html/gb-html-view.c \
	html/gb-html-view.h \
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
	tree/gb-tree-builder.c \
	tree/gb-tree-builder.h \
	tree/gb-tree.c \
	tree/gb-tree.h \
	tree/gb-tree-node.c \
	tree/gb-tree-node.h \
	tree/gb-tree-private.h \
	tree/gb-tree-types.h \
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
	util/gb-rgba.c \
	util/gb-rgba.h \
	util/gb-string.c \
	util/gb-string.h \
	util/gb-webkit.c \
	util/gb-webkit.h \
	util/gb-widget.c \
	util/gb-widget.h \
	views/gb-view-grid.c \
	views/gb-view-grid.h \
	views/gb-view-stack-actions.c \
	views/gb-view-stack-actions.h \
	views/gb-view-stack-private.h \
	views/gb-view-stack.c \
	views/gb-view-stack.h \
	views/gb-view.c \
	views/gb-view.h \
	workbench/gb-workbench-actions.c \
	workbench/gb-workbench-actions.h \
	workbench/gb-workbench-addin.c \
	workbench/gb-workbench-addin.h \
	workbench/gb-workbench-private.h \
	workbench/gb-workbench-types.h \
	workbench/gb-workbench.c \
	workbench/gb-workbench.h \
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

nodist_libgnome_builder_la_SOURCES = \
	resources/gb-resources.c \
	resources/gb-resources.h \
	$(NULL)

libgnome_builder_la_LIBADD = \
	$(BUILDER_LIBS) \
	$(OPTIMIZE_LDFLAGS) \
	$(top_builddir)/data/icons/hicolor/libicons.la \
	$(top_builddir)/libide/libide-1.0.la \
	$(top_builddir)/contrib/egg/libegg.la \
	$(top_builddir)/contrib/gd/libgd.la \
	$(top_builddir)/contrib/gedit/libgedit.la \
	$(top_builddir)/contrib/nautilus/libnautilus.la \
	$(top_builddir)/contrib/rg/librg.la \
	-lm

libgnome_builder_la_CFLAGS = \
	-DPACKAGE_DATADIR="\"${datadir}\"" \
	-DPACKAGE_LOCALE_DIR=\""${datadir}/locale"\" \
	$(BUILDER_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(OPTIMIZE_CFLAGS) \
	-I$(builddir)/resources \
	-I$(builddir)/util \
	-I$(srcdir)/app \
	-I$(srcdir)/css \
	-I$(srcdir)/dialogs \
	-I$(srcdir)/documents \
	-I$(srcdir)/editor \
	-I$(srcdir)/greeter \
	-I$(srcdir)/html \
	-I$(srcdir)/keybindings \
	-I$(srcdir)/preferences \
	-I$(srcdir)/project-tree \
	-I$(srcdir)/resources \
	-I$(srcdir)/scrolledwindow \
	-I$(srcdir)/search \
	-I$(srcdir)/support \
	-I$(srcdir)/tree \
	-I$(srcdir)/util \
	-I$(srcdir)/views \
	-I$(srcdir)/workbench \
	-I$(srcdir)/workspace \
	-I$(top_builddir)/plugins/devhelp \
	-I$(top_builddir)/data/icons/hicolor \
	-I$(top_builddir)/libide \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/contrib/egg \
	-I$(top_srcdir)/contrib/gd \
	-I$(top_srcdir)/contrib/gedit \
	-I$(top_srcdir)/contrib/nautilus \
	-I$(top_srcdir)/contrib/rg \
	$(NULL)

define_plugin = -Wl,--whole-archive,$(top_builddir)/plugins/$1/.libs/lib$1.a,--no-whole-archive

gnome_builder_PLUGINS = \
	devhelp \
	command-bar \
	device-manager \
	file-search \
	symbol-tree \
	sysmon \
	terminal \
	$(NULL)

gnome_builder_SOURCES = \
	gb-plugins.c \
	gb-plugins.h \
	main.c

gnome_builder_CFLAGS = $(libgnome_builder_la_CFLAGS)

gnome_builder_LDADD = \
	$(top_builddir)/libide/libide-1.0.la \
	$(foreach plugin,$(gnome_builder_PLUGINS),$(top_builddir)/plugins/$(plugin)/lib$(plugin).la) \
	libgnome-builder.la \
	$(NULL)


gnome_builder_LDFLAGS = \
	-export-dynamic \
	$(foreach plugin,$(gnome_builder_PLUGINS),$(call define_plugin,$(plugin))) \
	$(NULL)


glib_resources_c = resources/gb-resources.c
glib_resources_h = resources/gb-resources.h
glib_resources_xml = resources/gnome-builder.gresource.xml
glib_resources_namespace = gb
include $(top_srcdir)/build/autotools/Makefile.am.gresources

-include $(top_srcdir)/git.mk
