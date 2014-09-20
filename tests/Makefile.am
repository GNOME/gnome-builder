include ../build/autotools/automake/glib-tap.mk

noinst_PROGRAMS += test-c-parse-helper
TESTS += test-c-parse-helper
test_c_parse_helper_SOURCES = \
	$(top_srcdir)/src/editor/c-parse-helper.c \
	$(top_srcdir)/src/editor/c-parse-helper.h \
	$(top_srcdir)/src/log/gb-log.c \
	$(top_srcdir)/src/log/gb-log.h \
	test-c-parse-helper.c
test_c_parse_helper_CFLAGS = \
	$(GIO_CFLAGS) \
	-I$(top_srcdir)/src/editor \
	-I$(top_srcdir)/src/log
test_c_parse_helper_LDADD = $(GIO_LIBS)
