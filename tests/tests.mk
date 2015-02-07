noinst_PROGRAMS += test-c-parse-helper
TESTS += test-c-parse-helper
test_c_parse_helper_SOURCES = tests/test-c-parse-helper.c
test_c_parse_helper_CFLAGS = $(libgnome_builder_la_CFLAGS)
test_c_parse_helper_LDADD = libgnome-builder.la


noinst_PROGRAMS += test-navigation-list
TESTS += test-navigation-list
test_navigation_list_SOURCES = tests/test-navigation-list.c
test_navigation_list_CFLAGS = $(libgnome_builder_la_CFLAGS)
test_navigation_list_LDADD = libgnome-builder.la


noinst_PROGRAMS += test-ide-context
TESTS += test-ide-context
test_ide_context_SOURCES = tests/test-ide-context.c
test_ide_context_CFLAGS = \
	$(libide_la_CFLAGS) \
	-DTEST_DATA_DIR="\"$(top_srcdir)/tests/data\""
test_ide_context_LDADD = libide.la


EXTRA_DIST += \
	tests/data/project1/configure.ac \
	$(NULL)
