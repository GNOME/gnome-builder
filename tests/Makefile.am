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


TESTS_ENVIRONMENT= \
        GI_TYPELIB_PATH="$(top_srcdir)/libide"  \
        G_TEST_SRCDIR="$(abs_srcdir)" 		\
        G_TEST_BUILDDIR="$(abs_builddir)" 	\
        G_DEBUG=gc-friendly 			\
        MALLOC_CHECK_=2 			\
        MALLOC_PERTURB_=$$(($${RANDOM:-256} % 256))

LOG_COMPILER = $(top_srcdir)/build/autotools/tap-test

TESTS = test-ide-context
test_ide_context_SOURCES = test-ide-context.c
test_ide_context_CFLAGS = $(tests_cflags)
test_ide_context_LDADD = $(tests_libs)


TESTS += test-ide-back-forward-list
test_ide_back_forward_list_SOURCES = test-ide-back-forward-list.c
test_ide_back_forward_list_CFLAGS = $(tests_cflags)
test_ide_back_forward_list_LDADD = $(tests_libs)


TESTS += test-ide-buffer-manager
test_ide_buffer_manager_SOURCES = test-ide-buffer-manager.c
test_ide_buffer_manager_CFLAGS = $(tests_cflags)
test_ide_buffer_manager_LDADD = $(tests_libs)


TESTS += test-ide-buffer
test_ide_buffer_SOURCES = test-ide-buffer.c
test_ide_buffer_CFLAGS = $(tests_cflags)
test_ide_buffer_LDADD = $(tests_libs)


TESTS += test-ide-file-settings
test_ide_file_settings_SOURCES = test-ide-file-settings.c
test_ide_file_settings_CFLAGS = $(tests_cflags)
test_ide_file_settings_LDADD = $(tests_libs)


TESTS += test-ide-indenter
test_ide_indenter_SOURCES = test-ide-indenter.c
test_ide_indenter_CFLAGS = $(tests_cflags)
test_ide_indenter_LDADD = $(tests_libs)


TESTS += test-ide-vcs-uri
test_ide_vcs_uri_SOURCES = test-ide-vcs-uri.c
test_ide_vcs_uri_CFLAGS = $(tests_cflags)
test_ide_vcs_uri_LDADD = $(tests_libs)


TESTS += test-c-parse-helper
test_c_parse_helper_SOURCES = test-c-parse-helper.c
test_c_parse_helper_CFLAGS = \
	$(tests_cflags) \
	-include $(top_srcdir)/libide/c/c-parse-helper.c \
	-include $(top_srcdir)/libide/c/c-parse-helper.h \
	$(NULL)
test_c_parse_helper_LDADD = $(tests_libs)


TESTS += test-vim
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


misc_programs = test-ide-source-view
test_ide_source_view_SOURCES = test-ide-source-view.c
test_ide_source_view_CFLAGS = $(tests_cflags)
test_ide_source_view_LDADD = $(tests_libs)


TESTS += test-egg-state-machine
test_egg_state_machine_SOURCES = test-egg-state-machine.c
test_egg_state_machine_CFLAGS = $(tests_cflags) -I$(top_srcdir)/contrib/egg
test_egg_state_machine_LDADD = $(tests_libs) $(top_builddir)/contrib/egg/libegg.la

if ENABLE_TESTS
noinst_PROGRAMS = $(TESTS) $(misc_programs)
endif

check_PROGRAMS = $(TESTS) $(misc_programs)

EXTRA_DIST = \
	data/project1/configure.ac \
	data/project1/.editorconfig \
	$(NULL)

-include $(top_srcdir)/git.mk
