egg_cflags = \
	$(DEBUG_CFLAGS) \
	$(LIBIDE_CFLAGS) \
	-I$(top_srcdir)/contrib/egg \
	-DTEST_DATA_DIR="\"$(srcdir)/data\"" \
	-DBUILDDIR="\"$(builddir)\"" \
	$(NULL)

egg_libs = \
	$(LIBIDE_LIBS) \
	$(top_builddir)/contrib/egg/libegg.la \
	$(NULL)

rg_cflags = \
	$(DEBUG_CFLAGS) \
	$(RG_CFLAGS) \
	-I$(top_srcdir)/contrib/egg \
	-I$(top_srcdir)/contrib/rg \
	$(NULL)

rg_libs = \
	$(top_builddir)/contrib/egg/libegg.la \
	$(top_builddir)/contrib/rg/librg.la \
	$(RG_LIBS) \
	$(NULL)

search_cflags = \
	$(DEBUG_CFLAGS) \
	$(SEARCH_CFLAGS) \
	-I$(top_srcdir)/contrib/search \
	-I$(top_srcdir)/libide/util \
	$(NULL)

search_libs = \
	$(SEARCH_LIBS) \
	$(top_builddir)/contrib/search/libsearch.la \
	$(top_builddir)/libide/libide-1.0.la \
	$(NULL)

tests_cflags = \
	$(egg_cflags) \
	-I$(top_srcdir)/libide \
	-I$(top_builddir)/libide \
	$(NULL)

tests_libs = \
	$(egg_libs) \
	$(top_builddir)/libide/libide-1.0.la \
	$(NULL)

misc_programs =

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


TESTS += test-ide-doap
test_ide_doap_SOURCES = test-ide-doap.c
test_ide_doap_CFLAGS = $(tests_cflags)
test_ide_doap_LDADD = $(tests_libs)


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


misc_programs += test-ide-source-view
test_ide_source_view_SOURCES = test-ide-source-view.c
test_ide_source_view_CFLAGS = $(tests_cflags)
test_ide_source_view_LDADD = $(tests_libs)


misc_programs += test-cpu-graph
test_cpu_graph_SOURCES = test-cpu-graph.c
test_cpu_graph_CFLAGS = $(rg_cflags)
test_cpu_graph_LDADD = $(rg_libs)


misc_programs += test-fuzzy
test_fuzzy_SOURCES = test-fuzzy.c
test_fuzzy_CFLAGS = $(search_cflags)
test_fuzzy_LDADD = $(search_libs)


TESTS += test-ide-ctags
test_ide_ctags_SOURCES = test-ide-ctags.c
test_ide_ctags_CFLAGS = $(tests_cflags)
test_ide_ctags_LDADD = $(tests_libs)


TESTS += test-egg-binding-group
test_egg_binding_group_SOURCES = test-egg-binding-group.c
test_egg_binding_group_CFLAGS = $(egg_cflags)
test_egg_binding_group_LDADD = $(egg_libs)


TESTS += test-egg-signal-group
test_egg_signal_group_SOURCES = test-egg-signal-group.c
test_egg_signal_group_CFLAGS = $(egg_cflags)
test_egg_signal_group_LDADD = $(egg_libs)


TESTS += test-egg-state-machine
test_egg_state_machine_SOURCES = test-egg-state-machine.c
test_egg_state_machine_CFLAGS = $(egg_cflags)
test_egg_state_machine_LDADD = $(egg_libs)


TESTS += test-egg-cache
test_egg_cache_SOURCES = test-egg-cache.c
test_egg_cache_CFLAGS = $(egg_cflags)
test_egg_cache_LDADD = $(egg_libs)


TESTS += test-egg-heap
test_egg_heap_SOURCES = test-egg-heap.c
test_egg_heap_CFLAGS = $(egg_cflags)
test_egg_heap_LDADD = $(egg_libs)


if ENABLE_TESTS
noinst_PROGRAMS = $(TESTS) $(misc_programs)
endif

check_PROGRAMS = $(TESTS) $(misc_programs)

EXTRA_DIST = \
	data/project1/configure.ac \
	data/project1/.editorconfig \
	data/project1/project1.doap \
	data/project1/tags \
	$(NULL)

-include $(top_srcdir)/git.mk
