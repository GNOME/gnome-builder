noinst_LTLIBRARIES = libgedit.la

libgedit_la_SOURCES = \
	gedit-menu-stack-switcher.c \
	gedit-menu-stack-switcher.h \
	$(NULL)

libgedit_la_CFLAGS = \
	$(DEBUG_CFLAGS) \
	$(OPTIMIZE_CFLAGS) \
	$(GEDIT_CFLAGS) \
	$(NULL)

libgedit_la_LIBADD = \
	$(GEDIT_LIBADD) \
	$(NULL)

libgedit_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	$(NULL)

-include $(top_srcdir)/git.mk
