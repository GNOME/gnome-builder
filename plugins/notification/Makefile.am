if ENABLE_NOTIFICATION_PLUGIN

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libnotification-plugin.la
dist_plugin_DATA = notification.plugin

libnotification_plugin_la_SOURCES = \
	ide-notification-plugin.c   \
	ide-notification-addin.c    \
	ide-notification-addin.h    \
	$(NULL)

libnotification_plugin_la_CFLAGS = $(PLUGIN_CFLAGS)
libnotification_plugin_la_LDFLAGS = $(PLUGIN_LDFLAGS)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk
