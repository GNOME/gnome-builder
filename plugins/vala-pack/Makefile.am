if ENABLE_VALA_PACK_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libvala-pack-plugin.la
dist_plugin_DATA = vala-pack.plugin

libvala_pack_plugin_la_SOURCES = \
	ide-vala-service.vala \
	ide-vala-completion.vala \
	ide-vala-completion-item.vala \
	ide-vala-completion-provider.vala \
	ide-vala-diagnostics.vala \
	ide-vala-diagnostic-provider.vala \
	ide-vala-indenter.vala \
	ide-vala-index.vala \
	ide-vala-locator.vala \
	ide-vala-preferences-addin.vala \
	ide-vala-source-file.vala \
	ide-vala-symbol-resolver.vala \
	ide-vala-symbol-tree.vala \
	vala-pack-plugin.vala \
	$(NULL)

libvala_pack_plugin_la_VALAFLAGS = \
	--target-glib=2.44 \
	--thread \
	--vapidir $(top_builddir)/libide \
	--vapidir $(top_builddir)/contrib/egg \
	--vapidir $(top_builddir)/contrib/tmpl \
	--pkg gtksourceview-3.0 \
	--pkg libide-1.0 \
	--pkg libpeas-1.0 \
	--pkg libvala-$(VALA_VERSION) \
	--pkg posix \
	--pkg template-glib-1.0 \
	$(NULL)

libvala_pack_plugin_la_CFLAGS = \
	-DG_LOG_DOMAIN=\"vala-pack-plugin\" \
	-DGETTEXT_PACKAGE=\"gnome-builder\" \
	$(LIBIDE_CFLAGS) \
	$(VALA_CFLAGS) \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/contrib/egg \
	-I$(top_srcdir)/contrib/tmpl \
	-Wno-deprecated-declarations \
	-Wno-discarded-qualifiers \
	-Wno-implicit-function-declaration \
	-Wno-incompatible-pointer-types \
	-Wno-unused-but-set-variable \
	-Wno-unused-label \
	-Wno-unused-function \
	$(NULL)

libvala_pack_plugin_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	$(VALA_LIBS) \
	-avoid-version \
	-module \
	$(NULL)

include $(top_srcdir)/plugins/Makefile.plugin

GITIGNOREFILES = ide-vala.h

endif

-include $(top_srcdir)/git.mk
