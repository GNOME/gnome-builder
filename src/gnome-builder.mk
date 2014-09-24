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
	src/devhelp/gb-devhelp-workspace.c \
	src/devhelp/gb-devhelp-workspace.h \
	src/devhelp/gb-devhelp-tab.c \
	src/devhelp/gb-devhelp-tab.h \
	src/editor/c-parse-helper.c \
	src/editor/c-parse-helper.h \
	src/editor/gb-editor-commands.c \
	src/editor/gb-editor-commands.h \
	src/editor/gb-editor-document.c \
	src/editor/gb-editor-document.h \
	src/editor/gb-editor-navigation-item.c \
	src/editor/gb-editor-navigation-item.h \
	src/editor/gb-editor-settings.c \
	src/editor/gb-editor-settings.h \
	src/editor/gb-editor-tab.c \
	src/editor/gb-editor-tab.h \
	src/editor/gb-editor-tab-private.h \
	src/editor/gb-editor-workspace.c \
	src/editor/gb-editor-workspace.h \
	src/editor/gb-editor-workspace-private.h \
	src/editor/gb-source-auto-indenter.c \
	src/editor/gb-source-auto-indenter.h \
	src/editor/gb-source-auto-indenter-c.c \
	src/editor/gb-source-auto-indenter-c.h \
	src/editor/gb-source-change-monitor.c \
	src/editor/gb-source-change-monitor.h \
	src/editor/gb-source-formatter.c \
	src/editor/gb-source-formatter.h \
	src/editor/gb-source-change-gutter-renderer.c \
	src/editor/gb-source-change-gutter-renderer.h \
	src/editor/gb-source-search-highlighter.h \
	src/editor/gb-source-search-highlighter.c \
	src/markdown/gs-markdown.c \
	src/markdown/gs-markdown.h \
	src/markdown/gb-markdown-preview.c \
	src/markdown/gb-markdown-preview.h \
	src/navigation/gb-navigation-list.h \
	src/navigation/gb-navigation-list.c \
	src/navigation/gb-navigation-item.h \
	src/navigation/gb-navigation-item.c \
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
	src/theatrics/gb-box-theatric.c \
	src/theatrics/gb-box-theatric.h \
	src/trie/trie.c \
	src/trie/trie.h \
	src/util/gb-cairo.c \
	src/util/gb-cairo.h \
	src/util/gb-rgba.c \
	src/util/gb-rgba.h \
	src/util/gb-string.h \
	src/util/gb-widget.c \
	src/util/gb-widget.h \
	src/workbench/gb-workbench.c \
	src/workbench/gb-workbench.h \
	src/workbench/gb-workbench-actions.c \
	src/workbench/gb-workbench-actions.h \
	src/workbench/gb-workspace.c \
	src/workbench/gb-workspace.h

libgnome_builder_la_LIBADD = \
	$(DEVHELP_LIBS) \
	$(GIO_LIBS) \
	$(GTKSOURCEVIEW_LIBS) \
	$(GTK_LIBS) \
	$(WEBKIT_LIBS) \
	-lm

libgnome_builder_la_CFLAGS = \
	$(DEVHELP_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GTKSOURCEVIEW_CFLAGS) \
	$(GTK_CFLAGS) \
	$(MAINTAINER_CFLAGS) \
	$(WEBKIT_CFLAGS) \
	-I$(top_srcdir)/src/animation \
	-I$(top_srcdir)/src/app \
	-I$(top_srcdir)/src/devhelp \
	-I$(top_srcdir)/src/editor \
	-I$(top_srcdir)/src/gd \
	-I$(top_srcdir)/src/gedit \
	-I$(top_srcdir)/src/keybindings \
	-I$(top_srcdir)/src/log \
	-I$(top_srcdir)/src/markdown \
	-I$(top_srcdir)/src/nautilus \
	-I$(top_srcdir)/src/navigation \
	-I$(top_builddir)/src/resources \
	-I$(top_srcdir)/src/snippets \
	-I$(top_srcdir)/src/tabs \
	-I$(top_srcdir)/src/trie \
	-I$(top_srcdir)/src/theatrics \
	-I$(top_srcdir)/src/util \
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

src/app/libgnome_builder_la-gb-application.$(OBJEXT): $(gnome_builder_built_sources)

resource_files = $(shell glib-compile-resources --sourcedir=$(top_srcdir)/src/resources --generate-dependencies $(top_srcdir)/src/resources/gnome-builder.gresource.xml)
src/resources/gb-resources.c: src/resources/gnome-builder.gresource.xml $(resource_files)
	$(AM_V_GEN)glib-compile-resources --target=$@ --sourcedir=$(top_srcdir)/src/resources --generate-source --c-name gb $(top_srcdir)/src/resources/gnome-builder.gresource.xml
src/resources/gb-resources.h: src/resources/gnome-builder.gresource.xml $(resource_files)
	$(AM_V_GEN)glib-compile-resources --target=$@ --sourcedir=$(top_srcdir)/src/resources --generate-header --c-name gb $(top_srcdir)/src/resources/gnome-builder.gresource.xml

nodist_gnome_builder_SOURCES = \
	$(gnome_builder_built_sources) \
	$(NULL)

EXTRA_DIST += $(resource_files)
EXTRA_DIST += src/resources/gnome-builder.gresource.xml

CLEANFILES += $(gnome_builder_built_sources)
