pkglibdir = $(libdir)/gnome-builder
pkglib_LTLIBRARIES = libxml-private.la

libxml_private_la_SOURCES = \
	xml-reader.c \
	xml-reader.h \
	$(NULL)

libxml_private_la_CFLAGS = \
	$(DEBUG_CFLAGS) \
	$(OPTIMIZE_CFLAGS) \
	$(XML_CFLAGS) \
	$(NULL)

libxml_private_la_LIBADD = \
	$(XML_LIBS)
	$(NULL)

libxml_private_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	$(NULL)

-include $(top_srcdir)/git.mk
