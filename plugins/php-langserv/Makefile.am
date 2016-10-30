if ENABLE_PHP_LANGSERV_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
dist_plugin_DATA = \
	php-langserv.plugin \
	php_langserv_plugin.py

endif

include $(top_srcdir)/plugins/Makefile.plugin

-include $(top_srcdir)/git.mk
