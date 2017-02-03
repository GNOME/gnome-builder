if ENABLE_VALA_PACK_PLUGIN

# NOTE:
#
# This does not use the normal _SOURCES support for Vala in automake.
# This is because we need to ship files that are generated on the
# build machine, based on the vala it has available. These can change
# as has been seen in API breaks in libvala. Make sure you add any
# new .vala file to _VALASOURCES and the corresponding C file to
# _SOURCES.

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libvala-pack-plugin.la
dist_plugin_DATA = vala-pack.plugin

BUILT_SOURCES = libvala_pack_plugin_la_vala.stamp

libvala_pack_plugin_la_vala.stamp: $(libvala_pack_plugin_la_VALASOURCES)
	$(AM_V_GEN)$(VALAC) \
		$(libvala_pack_plugin_la_VALAFLAGS) \
		$(addprefix $(srcdir)/,$(libvala_pack_plugin_la_VALASOURCES)) \
		--ccode \
		&& touch $@

libvala_pack_plugin_la_VALASOURCES = \
	config.vapi \
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

EXTRA_DIST += $(libvala_pack_plugin_la_VALASOURCES)

nodist_libvala_pack_plugin_la_SOURCES = \
	ide-vala-service.c \
	ide-vala-completion.c \
	ide-vala-completion-item.c \
	ide-vala-completion-provider.c \
	ide-vala-diagnostics.c \
	ide-vala-diagnostic-provider.c \
	ide-vala-indenter.c \
	ide-vala-index.c \
	ide-vala-locator.c \
	ide-vala-preferences-addin.c \
	ide-vala-source-file.c \
	ide-vala-symbol-resolver.c \
	ide-vala-symbol-tree.c \
	vala-pack-plugin.c \
	$(NULL)

CLEANFILES = $(BUILT_SOURCES) $(nodist_libvala_pack_plugin_la_SOURCES)

libvala_pack_plugin_la_VALAFLAGS = \
	$(PLUGIN_VALAFLAGS) \
	--pkg gtksourceview-3.0 \
	--pkg libvala-$(VALA_VERSION) \
	--pkg posix \
	--pkg gio-2.0 \
	--pkg template-glib-1.0 \
	$(NULL)

if ENABLE_VALA_SYMBOL_GET_DEPRECATED
libvala_pack_plugin_la_VALAFLAGS += -D ENABLE_VALA_SYMBOL_GET_DEPRECATED
endif

if ENABLE_VALA_CODE_CONTEXT_SET_THREAD
libvala_pack_plugin_la_VALAFLAGS += -D ENABLE_VALA_CODE_CONTEXT_SET_THREAD
endif

libvala_pack_plugin_la_CFLAGS = \
	-DVALA_VERSION=\""$(VALA_VERSION)"\" \
	-DPACKAGE_DATADIR=\""$(datadir)/gnome-builder"\" \
	-DG_LOG_DOMAIN=\"vala-pack-plugin\" \
	-DGETTEXT_PACKAGE=\"gnome-builder\" \
	$(PLUGIN_CFLAGS) \
	$(VALA_CFLAGS) \
	-Wno-deprecated-declarations \
	-Wno-discarded-qualifiers \
	-Wno-implicit-function-declaration \
	-Wno-incompatible-pointer-types \
	-Wno-unused-but-set-variable \
	-Wno-unused-label \
	-Wno-unused-function \
	-Wno-unused-variable \
	$(NULL)

libvala_pack_plugin_la_LIBADD = $(VALA_LIBS)
libvala_pack_plugin_la_LDFLAGS = $(PLUGIN_LDFLAGS)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk
