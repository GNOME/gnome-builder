bin_PROGRAMS += gnome-builder
noinst_LTLIBRARIES += libgnome-builder.la

libgnome_builder_la_SOURCES = \
	$(gnome_builder_built_sources) \
	src/animation/gb-animation.c \
	src/animation/gb-animation.h \
	src/animation/gb-frame-source.c \
	src/animation/gb-frame-source.h \
	src/app/gb-application.c \
	src/app/gb-application.h \
	src/auto-indent/gb-source-auto-indenter.c \
	src/auto-indent/gb-source-auto-indenter.h \
	src/auto-indent/gb-source-auto-indenter-c.c \
	src/auto-indent/gb-source-auto-indenter-c.h \
	src/auto-indent/gb-source-auto-indenter-python.c \
	src/auto-indent/gb-source-auto-indenter-python.h \
	src/auto-indent/gb-source-auto-indenter-xml.c \
	src/auto-indent/gb-source-auto-indenter-xml.h \
	src/code-assistant/gb-source-code-assistant.h \
	src/code-assistant/gb-source-code-assistant.c \
	src/code-assistant/gb-source-code-assistant-renderer.c \
	src/code-assistant/gb-source-code-assistant-renderer.h \
	src/commands/gb-command.c \
	src/commands/gb-command.h \
	src/commands/gb-command-bar.c \
	src/commands/gb-command-bar.h \
	src/commands/gb-command-bar-item.c \
	src/commands/gb-command-bar-item.h \
	src/commands/gb-command-gaction-provider.c \
	src/commands/gb-command-gaction-provider.h \
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
	src/credits/gb-credits-widget.c \
	src/credits/gb-credits-widget.h \
	src/devhelp/gb-devhelp-tab.c \
	src/devhelp/gb-devhelp-tab.h \
	src/documents/gb-document.c \
	src/documents/gb-document.h \
	src/documents/gb-document-manager.c \
	src/documents/gb-document-manager.h \
	src/editor/c-parse-helper.c \
	src/editor/c-parse-helper.h \
	src/editor/gb-editor-document.c \
	src/editor/gb-editor-document.h \
	src/editor/gb-editor-file-mark.c \
	src/editor/gb-editor-file-mark.h \
	src/editor/gb-editor-file-marks.c \
	src/editor/gb-editor-file-marks.h \
	src/editor/gb-editor-frame.c \
	src/editor/gb-editor-frame.h \
	src/editor/gb-editor-frame-private.h \
	src/editor/gb-editor-navigation-item.c \
	src/editor/gb-editor-navigation-item.h \
	src/editor/gb-editor-settings-widget.c \
	src/editor/gb-editor-settings-widget.h \
	src/editor/gb-editor-tab.c \
	src/editor/gb-editor-tab.h \
	src/editor/gb-editor-tab-private.h \
	src/editor/gb-editor-workspace.c \
	src/editor/gb-editor-workspace.h \
	src/editor/gb-editor-workspace-private.h \
	src/editor/gb-source-change-monitor.c \
	src/editor/gb-source-change-monitor.h \
	src/editor/gb-source-formatter.c \
	src/editor/gb-source-formatter.h \
	src/editor/gb-source-change-gutter-renderer.c \
	src/editor/gb-source-change-gutter-renderer.h \
	src/editor/gb-source-highlight-menu.c \
	src/editor/gb-source-highlight-menu.h \
	src/editor/gb-source-search-highlighter.h \
	src/editor/gb-source-search-highlighter.c \
	src/editor/gb-source-style-scheme-button.c \
	src/editor/gb-source-style-scheme-button.h \
	src/editor/gb-source-style-scheme-widget.c \
	src/editor/gb-source-style-scheme-widget.h \
	src/gca/gca-diagnostics.c \
	src/gca/gca-diagnostics.h \
	src/gca/gca-service.c \
	src/gca/gca-service.h \
	src/gca/gca-structs.c \
	src/gca/gca-structs.h \
	src/markdown/gs-markdown.c \
	src/markdown/gs-markdown.h \
	src/markdown/gb-markdown-preview.c \
	src/markdown/gb-markdown-preview.h \
	src/markdown/gb-markdown-tab.c \
	src/markdown/gb-markdown-tab.h \
	src/navigation/gb-navigation-list.h \
	src/navigation/gb-navigation-list.c \
	src/navigation/gb-navigation-item.h \
	src/navigation/gb-navigation-item.c \
	src/preferences/gb-preferences-window.c \
	src/preferences/gb-preferences-window.h \
	src/preferences/gb-preferences-page.c \
	src/preferences/gb-preferences-page.h \
	src/preferences/gb-preferences-page-editor.c \
	src/preferences/gb-preferences-page-editor.h \
	src/preferences/gb-preferences-page-git.c \
	src/preferences/gb-preferences-page-git.h \
	src/preferences/gb-preferences-page-language.c \
	src/preferences/gb-preferences-page-language.h \
	src/sidebar/gb-sidebar.c \
	src/sidebar/gb-sidebar.h \
	src/snippets/gb-source-snippet-chunk.c \
	src/snippets/gb-source-snippet-chunk.h \
	src/snippets/gb-source-snippet-completion-item.c \
	src/snippets/gb-source-snippet-completion-item.h \
	src/snippets/gb-source-snippet-completion-provider.c \
	src/snippets/gb-source-snippet-completion-provider.h \
	src/snippets/gb-source-snippet-context.c \
	src/snippets/gb-source-snippet-context.h \
	src/snippets/gb-source-snippet.c \
	src/snippets/gb-source-snippet.h \
	src/snippets/gb-source-snippet-parser.c \
	src/snippets/gb-source-snippet-parser.h \
	src/snippets/gb-source-snippet-private.h \
	src/snippets/gb-source-snippets.c \
	src/snippets/gb-source-snippets.h \
	src/snippets/gb-source-snippets-manager.c \
	src/snippets/gb-source-snippets-manager.h \
	src/editor/gb-source-view.c \
	src/editor/gb-source-view.h \
	src/gd/gd-tagged-entry.c \
	src/gd/gd-tagged-entry.h \
	src/gedit/gedit-close-button.c \
	src/gedit/gedit-close-button.h \
	src/gedit/gedit-menu-stack-switcher.c \
	src/gedit/gedit-menu-stack-switcher.h \
	src/keybindings/gb-keybindings.c \
	src/keybindings/gb-keybindings.h \
	src/log/gb-log.c \
	src/log/gb-log.h \
	src/nautilus/nautilus-floating-bar.c \
	src/nautilus/nautilus-floating-bar.h \
	src/tabs/gb-multi-notebook.c \
	src/tabs/gb-multi-notebook.h \
	src/tabs/gb-notebook.c \
	src/tabs/gb-notebook.h \
	src/tabs/gb-tab-label.c \
	src/tabs/gb-tab-label.h \
	src/tabs/gb-tab-label-private.h \
	src/tabs/gb-tab.c \
	src/tabs/gb-tab.h \
	src/tabs/gb-tab-grid.c \
	src/tabs/gb-tab-grid.h \
	src/tabs/gb-tab-stack.c \
	src/tabs/gb-tab-stack.h \
	src/theatrics/gb-box-theatric.c \
	src/theatrics/gb-box-theatric.h \
	src/tree/gb-tree.c \
	src/tree/gb-tree.h \
	src/tree/gb-tree-builder.c \
	src/tree/gb-tree-builder.h \
	src/tree/gb-tree-node.c \
	src/tree/gb-tree-node.h \
	src/trie/trie.c \
	src/trie/trie.h \
	src/util/gb-cairo.c \
	src/util/gb-cairo.h \
	src/util/gb-doc-seq.c \
	src/util/gb-doc-seq.h \
	src/util/gb-gtk.c \
	src/util/gb-gtk.h \
	src/util/gb-rgba.c \
	src/util/gb-rgba.h \
	src/util/gb-string.h \
	src/util/gb-widget.c \
	src/util/gb-widget.h \
	src/vim/gb-source-vim.c \
	src/vim/gb-source-vim.h \
	src/workbench/gb-workbench.c \
	src/workbench/gb-workbench.h \
	src/workbench/gb-workbench-actions.c \
	src/workbench/gb-workbench-actions.h \
	src/workbench/gb-workspace.c \
	src/workbench/gb-workspace.h

libgnome_builder_la_LIBADD = \
	$(DEVHELP_LIBS) \
	$(GGIT_LIBS) \
	$(GIO_LIBS) \
	$(GTKSOURCEVIEW_LIBS) \
	$(GTK_LIBS) \
	$(WEBKIT_LIBS) \
	-lm

libgnome_builder_la_CFLAGS = \
	-DPACKAGE_DATADIR="\"$(datadir)\"" \
	$(DEVHELP_CFLAGS) \
	$(GGIT_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GTKSOURCEVIEW_CFLAGS) \
	$(GTK_CFLAGS) \
	$(MAINTAINER_CFLAGS) \
	$(WEBKIT_CFLAGS) \
	-I$(top_srcdir)/src/animation \
	-I$(top_srcdir)/src/app \
	-I$(top_srcdir)/src/auto-indent \
	-I$(top_srcdir)/src/commands \
	-I$(top_srcdir)/src/code-assistant \
	-I$(top_srcdir)/src/credits \
	-I$(top_srcdir)/src/devhelp \
	-I$(top_srcdir)/src/documents \
	-I$(top_srcdir)/src/editor \
	-I$(top_srcdir)/src/gca \
	-I$(top_srcdir)/src/gd \
	-I$(top_srcdir)/src/gedit \
	-I$(top_srcdir)/src/keybindings \
	-I$(top_srcdir)/src/log \
	-I$(top_srcdir)/src/markdown \
	-I$(top_srcdir)/src/nautilus \
	-I$(top_srcdir)/src/navigation \
	-I$(top_srcdir)/src/preferences \
	-I$(top_srcdir)/src/resources \
	-I$(top_builddir)/src/resources \
	-I$(top_srcdir)/src/sidebar \
	-I$(top_srcdir)/src/snippets \
	-I$(top_srcdir)/src/tabs \
	-I$(top_srcdir)/src/tree \
	-I$(top_srcdir)/src/trie \
	-I$(top_srcdir)/src/theatrics \
	-I$(top_srcdir)/src/util \
	-I$(top_srcdir)/src/vim \
	-I$(top_srcdir)/src/workbench

if ENABLE_TRACING
libgnome_builder_la_CFLAGS += -DGB_ENABLE_TRACE
endif

gnome_builder_SOURCES = src/main.c
gnome_builder_CFLAGS = $(libgnome_builder_la_CFLAGS)
gnome_builder_LDADD = libgnome-builder.la

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

DISTCLEANFILES += $(gnome_builder_built_sources)
