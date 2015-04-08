bin_PROGRAMS += gnome-builder
noinst_LTLIBRARIES += libgnome-builder.la

libgnome_builder_la_SOURCES = \
	$(gnome_builder_built_sources) \
	cut-n-paste/trie.c \
	cut-n-paste/trie.h \
	src/app/gb-application.c \
	src/app/gb-application.h \
	src/app/gb-application-actions.c \
	src/app/gb-application-actions.h \
	src/app/gb-application-credits.h \
	src/app/gb-application-private.h \
	src/commands/gb-command-bar-item.c \
	src/commands/gb-command-bar-item.h \
	src/commands/gb-command-bar.c \
	src/commands/gb-command-bar.h \
	src/commands/gb-command-gaction-provider.c \
	src/commands/gb-command-gaction-provider.h \
	src/commands/gb-command-gaction.c \
	src/commands/gb-command-gaction.h \
	src/commands/gb-command-manager.c \
	src/commands/gb-command-manager.h \
	src/commands/gb-command-provider.c \
	src/commands/gb-command-provider.h \
	src/commands/gb-command-result.c \
	src/commands/gb-command-result.h \
	src/commands/gb-command-vim-provider.c \
	src/commands/gb-command-vim-provider.h \
	src/commands/gb-command-vim.c \
	src/commands/gb-command-vim.h \
	src/commands/gb-command.c \
	src/commands/gb-command.h \
	src/devhelp/gb-devhelp-document.c \
	src/devhelp/gb-devhelp-document.h \
	src/devhelp/gb-devhelp-view.c \
	src/devhelp/gb-devhelp-view.h \
	src/dialogs/gb-new-project-dialog.c \
	src/dialogs/gb-new-project-dialog.h \
	src/dialogs/gb-projects-dialog.c \
	src/dialogs/gb-projects-dialog.h \
	src/dialogs/gb-recent-project-row.c \
	src/dialogs/gb-recent-project-row.h \
	src/documents/gb-document.c \
	src/documents/gb-document.h \
	src/editor/gb-editor-document.c \
	src/editor/gb-editor-document.h \
	src/editor/gb-editor-frame.c \
	src/editor/gb-editor-frame.h \
	src/editor/gb-editor-frame-actions.c \
	src/editor/gb-editor-frame-actions.h \
	src/editor/gb-editor-frame-private.h \
	src/editor/gb-editor-settings-widget.c \
	src/editor/gb-editor-settings-widget.h \
	src/editor/gb-editor-tweak-widget.c \
	src/editor/gb-editor-tweak-widget.h \
	src/editor/gb-editor-view.c \
	src/editor/gb-editor-view.h \
	src/editor/gb-editor-view-actions.c \
	src/editor/gb-editor-view-actions.h \
	src/editor/gb-editor-view-private.h \
	src/editor/gb-editor-workspace-actions.c \
	src/editor/gb-editor-workspace-actions.h \
	src/editor/gb-editor-workspace-private.h \
	src/editor/gb-editor-workspace.c \
	src/editor/gb-editor-workspace.h \
	src/gd/gd-tagged-entry.c \
	src/gd/gd-tagged-entry.h \
	src/gedit/gedit-close-button.c \
	src/gedit/gedit-close-button.h \
	src/gedit/gedit-menu-stack-switcher.c \
	src/gedit/gedit-menu-stack-switcher.h \
	src/html/gb-html-document.c \
	src/html/gb-html-document.h \
	src/html/gb-html-view.c \
	src/html/gb-html-view.h \
	src/keybindings/gb-keybindings.c \
	src/keybindings/gb-keybindings.h \
	src/nautilus/nautilus-floating-bar.c \
	src/nautilus/nautilus-floating-bar.h \
	src/preferences/gb-preferences-page-editor.c \
	src/preferences/gb-preferences-page-editor.h \
	src/preferences/gb-preferences-page-experimental.c \
	src/preferences/gb-preferences-page-experimental.h \
	src/preferences/gb-preferences-page-git.c \
	src/preferences/gb-preferences-page-git.h \
	src/preferences/gb-preferences-page-keybindings.c \
	src/preferences/gb-preferences-page-keybindings.h \
	src/preferences/gb-preferences-page-language.c \
	src/preferences/gb-preferences-page-language.h \
	src/preferences/gb-preferences-page.c \
	src/preferences/gb-preferences-page.h \
	src/preferences/gb-preferences-window.c \
	src/preferences/gb-preferences-window.h \
	src/scrolledwindow/gb-scrolled-window.c \
	src/scrolledwindow/gb-scrolled-window.h \
	src/search/gb-search-box.c \
	src/search/gb-search-box.h \
	src/search/gb-search-display-group.c \
	src/search/gb-search-display-group.h \
	src/search/gb-search-display-row.c \
	src/search/gb-search-display-row.h \
	src/search/gb-search-display.c \
	src/search/gb-search-display.h \
	src/support/gb-support.c \
	src/support/gb-support.h \
	src/tree/gb-project-tree-builder.c \
	src/tree/gb-project-tree-builder.h \
	src/tree/gb-tree-builder.c \
	src/tree/gb-tree-builder.h \
	src/tree/gb-tree-node.c \
	src/tree/gb-tree-node.h \
	src/tree/gb-tree.c \
	src/tree/gb-tree.h \
	src/util/gb-cairo.c \
	src/util/gb-cairo.h \
	src/util/gb-dnd.c \
	src/util/gb-dnd.h \
	src/util/gb-glib.c \
	src/util/gb-glib.h \
	src/util/gb-gtk.c \
	src/util/gb-gtk.h \
	src/util/gb-nautilus.c \
	src/util/gb-nautilus.h \
	src/util/gb-pango.c \
	src/util/gb-pango.h \
	src/util/gb-rgba.c \
	src/util/gb-rgba.h \
	src/util/gb-string.c \
	src/util/gb-string.h \
	src/util/gb-widget.c \
	src/util/gb-widget.h \
	src/views/gb-view-grid.c \
	src/views/gb-view-grid.h \
	src/views/gb-view-stack-actions.c \
	src/views/gb-view-stack-actions.h \
	src/views/gb-view-stack-private.h \
	src/views/gb-view-stack.c \
	src/views/gb-view-stack.h \
	src/views/gb-view.c \
	src/views/gb-view.h \
	src/vim/gb-vim.c \
	src/vim/gb-vim.h \
	src/workbench/gb-workbench-actions.c \
	src/workbench/gb-workbench-actions.h \
	src/workbench/gb-workbench-private.h \
	src/workbench/gb-workbench-types.h \
	src/workbench/gb-workbench.c \
	src/workbench/gb-workbench.h \
	src/workbench/gb-workspace.c \
	src/workbench/gb-workspace.h \
	$(NULL)

disabled_files = \
	src/editor/gb-source-formatter.c \
	src/editor/gb-source-formatter.h \
	src/editor/gb-source-highlight-menu.c \
	src/editor/gb-source-highlight-menu.h \
	$(NULL)

libgnome_builder_la_LIBADD = \
	$(BUILDER_LIBS) \
	libide-1.0.la \
	-lm

libgnome_builder_la_CFLAGS = \
	-DPACKAGE_DATADIR="\"${datadir}\"" \
	-DPACKAGE_LOCALE_DIR=\""${datadir}/locale"\" \
	$(BUILDER_CFLAGS) \
	$(MAINTAINER_CFLAGS) \
	-I$(top_builddir)/src/resources \
	-I$(top_builddir)/src/util \
	-I$(top_builddir)/libide \
	-I$(top_srcdir)/cut-n-paste \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/src/app \
	-I$(top_srcdir)/src/commands \
	-I$(top_srcdir)/src/devhelp \
	-I$(top_srcdir)/src/dialogs \
	-I$(top_srcdir)/src/documents \
	-I$(top_srcdir)/src/editor \
	-I$(top_srcdir)/src/gd \
	-I$(top_srcdir)/src/gedit \
	-I$(top_srcdir)/src/html \
	-I$(top_srcdir)/src/keybindings \
	-I$(top_srcdir)/src/nautilus \
	-I$(top_srcdir)/src/preferences \
	-I$(top_srcdir)/src/resources \
	-I$(top_srcdir)/src/scrolledwindow \
	-I$(top_srcdir)/src/search \
	-I$(top_srcdir)/src/support \
	-I$(top_srcdir)/src/tree \
	-I$(top_srcdir)/src/util \
	-I$(top_srcdir)/src/views \
	-I$(top_srcdir)/src/vim \
	-I$(top_srcdir)/src/workbench

if ENABLE_TRACING
libgnome_builder_la_CFLAGS += -DGB_ENABLE_TRACE
else
libgnome_builder_la_CFLAGS += -DIDE_DISABLE_TRACE
endif

gnome_builder_SOURCES = src/main.c
gnome_builder_CFLAGS = $(libgnome_builder_la_CFLAGS)
gnome_builder_LDADD = \
	libide-1.0.la \
	libgnome-builder.la \
	$(NULL)

# XXX: Workaround for now, need to find a more automated way to do this
# in how we build projects inside of Builder.
gnome_builder_built_sources = \
	src/resources/gb-resources.c \
	src/resources/gb-resources.h

resource_files = $(shell glib-compile-resources --sourcedir=$(top_srcdir)/src/resources --generate-dependencies $(top_srcdir)/src/resources/gnome-builder.gresource.xml)
src/resources/gb-resources.c: src/resources/gnome-builder.gresource.xml $(resource_files)
	$(AM_V_GEN)glib-compile-resources --target=$@ --sourcedir=$(top_srcdir)/src/resources --generate-source --c-name gb $(top_srcdir)/src/resources/gnome-builder.gresource.xml
src/resources/gb-resources.h: src/resources/gnome-builder.gresource.xml $(resource_files)
	$(AM_V_GEN)glib-compile-resources --target=$@ --sourcedir=$(top_srcdir)/src/resources --generate-header --c-name gb $(top_srcdir)/src/resources/gnome-builder.gresource.xml

nodist_gnome_builder_SOURCES = \
	$(gnome_builder_built_sources) \
	$(NULL)

BUILT_SOURCES += $(gnome_builder_built_sources)

EXTRA_DIST += $(resource_files)
EXTRA_DIST += src/resources/gnome-builder.gresource.xml
EXTRA_DIST += $(gnome_builder_built_sources)
EXTRA_DIST += $(disabled_files)

DISTCLEANFILES += $(gnome_builder_built_sources)

