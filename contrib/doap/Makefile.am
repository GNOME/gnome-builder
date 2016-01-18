noinst_LTLIBRARIES = libdoap.la

libdoap_la_CPPFLAGS = \
	-I$(top_srcdir)/contrib/xml \
	$(NULL)

libdoap_la_SOURCES = \
	doap-document.c \
	doap-document.h \
	doap-person.c \
	doap-person.h \
	$(NULL)

libdoap_la_CFLAGS = \
	$(XML_CFLAGS)

libdoap_la_LIBADD = \
	$(XML_LIBS) \
	$(top_builddir)/contrib/xml/libxml.la

-include $(top_srcdir)/git.mk
