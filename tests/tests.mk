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
