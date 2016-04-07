if ENABLE_HELLO_CPP_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libhello-cpp-plugin.la
dist_plugin_DATA = hello-cpp.plugin

libhello_cpp_plugin_la_SOURCES = \
	hellocppapplicationaddin.cc \
	hellocppapplicationaddin.h \
	hellocppplugin.cc \
	registermmtype.h \
	$(NULL)

libhello_cpp_plugin_la_CPPFLAGS = $(IDEMM_CFLAGS) $(PLUGIN_CFLAGS) -I$(top_srcdir)/libidemm
libhello_cpp_plugin_la_LDFLAGS = $(IDEMM_LIBS) $(PLUGIN_LDFLAGS) -L$(top_srcdir)/libidemm/idemm/.libs -lidemm-1.0

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk
