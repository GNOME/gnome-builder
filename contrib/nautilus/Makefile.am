noinst_LTLIBRARIES = libnautilus.la

libnautilus_la_SOURCES =        \
	nautilus-floating-bar.c \
	nautilus-floating-bar.h \
	$(NULL)

libnautilus_la_CFLAGS =         \
	$(DEBUG_CFLAGS)         \
	$(OPTIMIZE_CFLAGS)      \
	$(NAUTILUS_CFLAGS)      \
	$(NULL)

libnautilus_la_LIBADD =         \
	$(NAUTILUS_LIBADD)      \
	$(NULL)

libnautilus_la_LDFLAGS =        \
	$(OPTIMIZE_LDFLAGS)     \
	$(NULL)

-include $(top_srcdir)/git.mk
