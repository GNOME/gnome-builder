EXTRA_DIST =

egg_cflags = \
	$(DEBUG_CFLAGS) \
	$(LIBIDE_CFLAGS) \
	-I$(top_srcdir)/contrib/egg \
	-DTEST_DATA_DIR="\"$(srcdir)/data\"" \
	$(NULL)

egg_libs = \
	$(LIBIDE_LIBS) \
	$(top_builddir)/contrib/egg/libegg-private.la \
	$(NULL)

rg_cflags = \
	$(DEBUG_CFLAGS) \
	$(RG_CFLAGS) \
	-I$(top_srcdir)/contrib/egg \
	-I$(top_srcdir)/contrib/rg \
	$(NULL)

rg_libs = \
	$(top_builddir)/contrib/egg/libegg-private.la \
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
	$(top_builddir)/contrib/egg/libegg-private.la \
	$(NULL)

tests_cflags = \
	$(egg_cflags) \
	$(LIBIDE_CFLAGS) \
	-I$(top_srcdir)/libide \
	-I$(top_builddir)/libide \
	-I$(top_srcdir)/contrib/pnl \
	-I$(top_builddir)/contrib/pnl \
	$(NULL)

tests_libs = \
	$(LIBIDE_LIBS) \
	$(top_builddir)/libide/libide-1.0.la \
	$(top_builddir)/contrib/egg/libegg-private.la \
	$(NULL)

tests_ldflags = \
	-export-dynamic \
	$(NULL)

misc_programs =

TESTS_ENVIRONMENT= \
	GI_TYPELIB_PATH="$(top_builddir)/libide:$(top_builddir)/contrib/tmpl:$(top_builddir)/contrib/egg:$(GI_TYPELIB_PATH)" \
	GB_IN_TREE_PLUGINS=1 \
	G_TEST_SRCDIR="$(abs_srcdir)" \
	G_TEST_BUILDDIR="$(abs_builddir)" \
	G_DEBUG=gc-friendly \
	GSETTINGS_BACKEND=memory \
	PYTHONDONTWRITEBYTECODE=yes \
	MALLOC_CHECK_=2 \
	MALLOC_PERTURB_=$$(($${RANDOM:-256} % 256))

LOG_COMPILER = $(top_srcdir)/build/autotools/tap-test

TESTS = test-ide-context
test_ide_context_SOURCES = test-ide-context.c
test_ide_context_CFLAGS = $(tests_cflags)
test_ide_context_LDADD = $(tests_libs)
test_ide_context_LDFLAGS = $(tests_ldflags)


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


#TESTS += test-c-parse-helper
#test_c_parse_helper_SOURCES = test-c-parse-helper.c
#test_c_parse_helper_CFLAGS = \
#	$(tests_cflags) \
#	-include $(top_srcdir)/libide/c/c-parse-helper.c \
#	-include $(top_srcdir)/libide/c/c-parse-helper.h \
#	$(NULL)
#test_c_parse_helper_LDADD = $(tests_libs)


TESTS += test-vim
test_vim_SOURCES = test-vim.c
test_vim_CFLAGS = $(tests_cflags)
test_vim_LDADD = $(tests_libs)


misc_programs += test-cpu-graph
test_cpu_graph_SOURCES = test-cpu-graph.c
test_cpu_graph_CFLAGS = $(rg_cflags)
test_cpu_graph_LDADD = $(rg_libs)


misc_programs += test-fuzzy
test_fuzzy_SOURCES = test-fuzzy.c
test_fuzzy_CFLAGS = $(search_cflags)
test_fuzzy_LDADD = $(search_libs)


misc_programs += test-egg-slider
test_egg_slider_SOURCES = test-egg-slider.c
test_egg_slider_CFLAGS = $(egg_cflags)
test_egg_slider_LDADD = $(egg_libs)


misc_programs += test-snippet-parser
test_snippet_parser_SOURCES = test-snippet-parser.c
test_snippet_parser_CFLAGS = $(tests_cflags)
test_snippet_parser_LDADD = $(tests_libs)


#TESTS += test-ide-ctags
#test_ide_ctags_SOURCES = test-ide-ctags.c
#test_ide_ctags_CFLAGS = $(tests_cflags)
#test_ide_ctags_LDADD = $(tests_libs)


TESTS += test-egg-binding-group
test_egg_binding_group_SOURCES = test-egg-binding-group.c
test_egg_binding_group_CFLAGS = $(egg_cflags)
test_egg_binding_group_LDADD = $(egg_libs)

misc_programs += test-egg-menu-manager
test_egg_menu_manager_SOURCES = test-egg-menu-manager.c
test_egg_menu_manager_CFLAGS = $(egg_cflags)
test_egg_menu_manager_LDADD = $(egg_libs)
EXTRA_DIST += \
	menus-exten-1.ui \
	menus-exten-2.ui \
	menus-exten-3.ui \
	menus-exten-4.ui \
	menus-exten-5.ui \
	menus.ui


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

EXTRA_DIST += \
	data/project1/configure.ac \
	data/project1/.editorconfig \
	data/project1/project1.doap \
	data/project1/tags \
	$(NULL)

-include $(top_srcdir)/git.mk
