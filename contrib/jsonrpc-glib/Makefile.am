noinst_LTLIBRARIES = libjsonrpc-glib.la

libjsonrpc_glib_la_SOURCES = \
	jcon.c \
	jcon.h \
	jsonrpc-client.c \
	jsonrpc-client.h \
	jsonrpc-glib.h \
	jsonrpc-input-stream.c \
	jsonrpc-input-stream.h \
	jsonrpc-output-stream.c \
	jsonrpc-output-stream.h \
	jsonrpc-version.h \
	$(NULL)

libjsonrpc_glib_la_CFLAGS = \
	-DJSONRPC_GLIB_INSIDE \
	$(DEBUG_CFLAGS) \
	$(JSONRPC_CFLAGS) \
	$(OPTIMIZE_CFLAGS) \
	$(NULL)

libjsonrpc_glib_la_LIBADD = $(JSONRPC_LIBS)
libjsonrpc_glib_la_LDFLAGS = $(OPTIMIZE_LDFLAGS)

-include $(top_srcdir)/git.mk
