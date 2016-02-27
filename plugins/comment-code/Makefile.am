if ENABLE_COMMENT_CODE_PLUGIN

DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libcomment-code-plugin.la
dist_plugin_DATA = comment-code.plugin

libcomment_code_plugin_la_SOURCES = \
	gbp-comment-code-plugin.c \
	gbp-comment-code-view-addin.c \
	gbp-comment-code-view-addin.h \
	$(NULL)

nodist_libcomment_code_plugin_la_SOURCES = \
	gbp-comment-code-resources.c \
	gbp-comment-code-resources.h

libcomment_code_plugin_la_CFLAGS = \
	$(LIBIDE_CFLAGS) \
	$(OPTIMIZE_CFLAGS) \
	-I$(top_srcdir)/libide \
	$(NULL)

libcomment_code_plugin_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-module \
	-export-regex peas_register_types \
	$(NULL)

glib_resources_c = gbp-comment-code-resources.c
glib_resources_h = gbp-comment-code-resources.h
glib_resources_xml = gbp-comment-code.gresource.xml
glib_resources_namespace = gbp_comment_code
include $(top_srcdir)/build/autotools/Makefile.am.gresources

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk
