noinst_LTLIBRARIES = libdoap.la

libdoap_la_CPPFLAGS = \
	-I$(top_srcdir)/contrib/xml \
	$(NULL)

libdoap_la_SOURCES = \
	ide-doap.c \
	ide-doap.h \
	ide-doap-person.c \
	ide-doap-person.h \
	$(NULL)

libdoap_la_CFLAGS = \
	$(XML_CFLAGS)

libdoap_la_LIBADD = \
	$(XML_LIBS) \
	$(top_builddir)/contrib/xml/libxml.la

-include $(top_srcdir)/git.mk
