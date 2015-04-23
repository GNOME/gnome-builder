noinst_PROGRAMS += test-c-parse-helper
TESTS += test-c-parse-helper
test_c_parse_helper_SOURCES = tests/test-c-parse-helper.c libide/c/c-parse-helper.c
test_c_parse_helper_CFLAGS = $(libide_1_0_la_CFLAGS) -I$(top_srcdir)/libide/c/
test_c_parse_helper_LDADD = $(LIBIDE_LIBS)


noinst_PROGRAMS += test-ide-context
TESTS += test-ide-context
test_ide_context_SOURCES = tests/test-ide-context.c
test_ide_context_CFLAGS = \
	$(libide_1_0_la_CFLAGS) \
	-DTEST_DATA_DIR="\"$(top_srcdir)/tests/data\"" \
	-DBUILDDIR="\"$(abs_top_builddir)\""
test_ide_context_LDADD = libide-1.0.la $(LIBIDE_LIBS)


noinst_PROGRAMS += test-ide-back-forward-list
TESTS += test-ide-back-forward-list
test_ide_back_forward_list_SOURCES = tests/test-ide-back-forward-list.c
test_ide_back_forward_list_CFLAGS = \
	$(libide_1_0_la_CFLAGS) \
	-DTEST_DATA_DIR="\"$(top_srcdir)/tests/data\"" \
	-DBUILDDIR="\"$(abs_top_builddir)\""
test_ide_back_forward_list_LDADD = libide-1.0.la $(LIBIDE_LIBS)


noinst_PROGRAMS += test-ide-buffer-manager
TESTS += test-ide-buffer-manager
test_ide_buffer_manager_SOURCES = tests/test-ide-buffer-manager.c
test_ide_buffer_manager_CFLAGS = \
	$(libide_1_0_la_CFLAGS) \
	-DTEST_DATA_DIR="\"$(top_srcdir)/tests/data\"" \
	-DBUILDDIR="\"$(abs_top_builddir)\""
test_ide_buffer_manager_LDADD = libide-1.0.la $(LIBIDE_LIBS)


noinst_PROGRAMS += test-ide-buffer
TESTS += test-ide-buffer
test_ide_buffer_SOURCES = tests/test-ide-buffer.c
test_ide_buffer_CFLAGS = \
	$(libide_1_0_la_CFLAGS) \
	-DTEST_DATA_DIR="\"$(top_srcdir)/tests/data\"" \
	-DBUILDDIR="\"$(abs_top_builddir)\""
test_ide_buffer_LDADD = libide-1.0.la $(LIBIDE_LIBS)


noinst_PROGRAMS += test-ide-file-settings
TESTS += test-ide-file-settings
test_ide_file_settings_SOURCES = tests/test-ide-file-settings.c
test_ide_file_settings_CFLAGS = \
	$(libide_1_0_la_CFLAGS) \
	-DTEST_DATA_DIR="\"$(top_srcdir)/tests/data\"" \
	-DBUILDDIR="\"$(abs_top_builddir)\""
test_ide_file_settings_LDADD = libide-1.0.la $(LIBIDE_LIBS)


noinst_PROGRAMS += test-ide-indenter
TESTS += test-ide-indenter
test_ide_indenter_SOURCES = tests/test-ide-indenter.c
test_ide_indenter_CFLAGS = \
	$(libide_1_0_la_CFLAGS) \
	-DTEST_DATA_DIR="\"$(top_srcdir)/tests/data\"" \
	-DBUILDDIR="\"$(abs_top_builddir)\""
test_ide_indenter_LDADD = libide-1.0.la $(LIBIDE_LIBS)


noinst_PROGRAMS += test-vim
TESTS += test-vim
test_vim_SOURCES = tests/test-vim.c
test_vim_CFLAGS = \
	-I$(top_builddir)/src/resources \
	$(libide_1_0_la_CFLAGS) \
	-DTEST_DATA_DIR="\"$(top_srcdir)/tests/data\"" \
	-DBUILDDIR="\"$(abs_top_builddir)\""
test_vim_LDADD = \
	libide-1.0.la \
	libgnome-builder.la \
	$(LIBIDE_LIBS) \
	$(NULL)


noinst_PROGRAMS += test-ide-source-view
test_ide_source_view_SOURCES = tests/test-ide-source-view.c
test_ide_source_view_CFLAGS = \
	$(libide_1_0_la_CFLAGS) \
	-DTEST_DATA_DIR="\"$(top_srcdir)/tests/data\"" \
	-DBUILDDIR="\"$(abs_top_builddir)\""
test_ide_source_view_LDADD = libide-1.0.la $(LIBIDE_LIBS)


noinst_PROGRAMS += test-ide-vcs-uri
test_ide_vcs_uri_SOURCES = tests/test-ide-vcs-uri.c
test_ide_vcs_uri_CFLAGS = \
	$(libide_1_0_la_CFLAGS) \
	-DTEST_DATA_DIR="\"$(top_srcdir)/tests/data\"" \
	-DBUILDDIR="\"$(abs_top_builddir)\""
test_ide_vcs_uri_LDADD = libide-1.0.la $(LIBIDE_LIBS)


EXTRA_DIST += \
	tests/data/project1/configure.ac \
	tests/data/project1/.editorconfig \
	tests/tests.h \
	$(NULL)
