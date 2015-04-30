include $(top_srcdir)/build/autotools/Makefile.am.glib-tap


tests_cflags = \
	-I$(top_srcdir)/libide \
	-I$(top_builddir)/libide \
	$(DEBUG_CFLAGS) \
	$(LIBIDE_CFLAGS) \
	-DTEST_DATA_DIR="\"$(srcdir)/data\"" \
	-DBUILDDIR="\"$(builddir)\"" \
	$(NULL)

tests_libs = \
	$(LIBIDE_LIBS) \
	$(top_builddir)/libide/libide-1.0.la \
	$(NULL)


TESTS += test-ide-context
noinst_PROGRAMS += test-ide-context
test_ide_context_SOURCES = test-ide-context.c
test_ide_context_CFLAGS = $(tests_cflags)
test_ide_context_LDADD = $(tests_libs)


TESTS += test-ide-back-forward-list
noinst_PROGRAMS += test-ide-back-forward-list
test_ide_back_forward_list_SOURCES = test-ide-back-forward-list.c
test_ide_back_forward_list_CFLAGS = $(tests_cflags)
test_ide_back_forward_list_LDADD = $(tests_libs)


TESTS += test-ide-buffer-manager
noinst_PROGRAMS += test-ide-buffer-manager
test_ide_buffer_manager_SOURCES = test-ide-buffer-manager.c
test_ide_buffer_manager_CFLAGS = $(tests_cflags)
test_ide_buffer_manager_LDADD = $(tests_libs)


TESTS += test-ide-buffer
noinst_PROGRAMS += test-ide-buffer
test_ide_buffer_SOURCES = test-ide-buffer.c
test_ide_buffer_CFLAGS = $(tests_cflags)
test_ide_buffer_LDADD = $(tests_libs)


TESTS += test-ide-file-settings
noinst_PROGRAMS += test-ide-file-settings
test_ide_file_settings_SOURCES = test-ide-file-settings.c
test_ide_file_settings_CFLAGS = $(tests_cflags)
test_ide_file_settings_LDADD = $(tests_libs)


TESTS += test-ide-indenter
noinst_PROGRAMS += test-ide-indenter
test_ide_indenter_SOURCES = test-ide-indenter.c
test_ide_indenter_CFLAGS = $(tests_cflags)
test_ide_indenter_LDADD = $(tests_libs)


TESTS += test-ide-vcs-uri
noinst_PROGRAMS += test-ide-vcs-uri
test_ide_vcs_uri_SOURCES = test-ide-vcs-uri.c
test_ide_vcs_uri_CFLAGS = $(tests_cflags)
test_ide_vcs_uri_LDADD = $(tests_libs)


TESTS += test-c-parse-helper
noinst_PROGRAMS += test-c-parse-helper
test_c_parse_helper_SOURCES = test-c-parse-helper.c
test_c_parse_helper_CFLAGS = \
	$(tests_cflags) \
	-include $(top_srcdir)/libide/c/c-parse-helper.c \
	-include $(top_srcdir)/libide/c/c-parse-helper.h \
	$(NULL)
test_c_parse_helper_LDADD = $(tests_libs)


TESTS += test-vim
noinst_PROGRAMS += test-vim
test_vim_SOURCES = test-vim.c
test_vim_CFLAGS = \
	$(tests_cflags) \
	-I$(top_srcdir)/src/resources \
	-I$(top_builddir)/src/resources \
	$(NULL)
test_vim_LDADD = \
	$(tests_libs) \
	$(top_builddir)/src/libgnome-builder.la \
	$(NULL)


noinst_PROGRAMS += test-ide-source-view
test_ide_source_view_SOURCES = test-ide-source-view.c
test_ide_source_view_CFLAGS = $(tests_cflags)
test_ide_source_view_LDADD = $(tests_libs)


EXTRA_DIST += \
	data/project1/configure.ac \
	data/project1/.editorconfig \
	$(NULL)

-include $(top_srcdir)/git.mk
