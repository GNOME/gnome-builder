EXTRA_DIST =

gstyle_cflags =                              \
	$(DEBUG_CFLAGS)                      \
	$(GSTYLE_CFLAGS)                     \
	-I$(top_srcdir)/contrib/gstyle       \
	-DTEST_DATA_DIR="\"$(abs_srcdir)/data\"" \
	$(NULL)

gstyle_libs =                                               \
	$(GSTYLE_LIBS)                                      \
	$(top_builddir)/contrib/gstyle/libgstyle-private.la \
	$(NULL)

tests_cflags =                   \
	$(egg_cflags)            \
	$(gstyle_cflags)         \
	$(NULL)

tests_libs =                                          \
	$(gstyle_libs)                                \
	$(top_builddir)/contrib/egg/libegg-private.la \
	$(NULL)

tests_ldflags =         \
	-export-dynamic \
	$(NULL)

misc_programs =

TESTS_ENVIRONMENT =                                 \
	GB_IN_TREE_PLUGINS=1                        \
	G_TEST_SRCDIR="$(abs_srcdir)"               \
	G_TEST_BUILDDIR="$(abs_builddir)"           \
	G_DEBUG=gc-friendly                         \
	GSETTINGS_BACKEND=memory                    \
	MALLOC_CHECK_=2                             \
	MALLOC_PERTURB_=$$(($${RANDOM:-256} % 256)) \
	$(NULL)

LOG_COMPILER = $(top_srcdir)/build/autotools/tap-test

misc_programs += test-gstyle-color
test_gstyle_color_SOURCES = test-gstyle-color.c
test_gstyle_color_CFLAGS = $(tests_cflags)
test_gstyle_color_LDADD =  $(tests_libs)

misc_programs += test-gstyle-color-panel
test_gstyle_color_panel_SOURCES = test-gstyle-color-panel.c
test_gstyle_color_panel_CFLAGS = $(tests_cflags)
test_gstyle_color_panel_LDADD = $(tests_libs)

misc_programs += test-gstyle-color-plane
test_gstyle_color_plane_SOURCES = test-gstyle-color-plane.c
test_gstyle_color_plane_CFLAGS = $(tests_cflags)
test_gstyle_color_plane_LDADD = $(tests_libs)

misc_programs += test-gstyle-color-scale
test_gstyle_color_scale_SOURCES = test-gstyle-color-scale.c
test_gstyle_color_scale_CFLAGS = $(tests_cflags)
test_gstyle_color_scale_LDADD = $(tests_libs)

misc_programs += test-gstyle-color-widget
test_gstyle_color_widget_SOURCES = test-gstyle-color-widget.c
test_gstyle_color_widget_CFLAGS = $(tests_cflags)
test_gstyle_color_widget_LDADD = $(tests_libs)

misc_programs += test-gstyle-filter
test_gstyle_filter_SOURCES = test-gstyle-filter.c
test_gstyle_filter_CFLAGS = $(tests_cflags)
test_gstyle_filter_LDADD = $(tests_libs)

misc_programs += test-gstyle-palette
test_gstyle_palette_SOURCES = test-gstyle-palette.c
test_gstyle_palette_CFLAGS = $(tests_cflags)
test_gstyle_palette_LDADD = $(tests_libs)

misc_programs += test-gstyle-palette-widget
test_gstyle_palette_widget_SOURCES = test-gstyle-palette-widget.c
test_gstyle_palette_widget_CFLAGS = $(tests_cflags)
test_gstyle_palette_widget_LDADD = $(tests_libs)

misc_programs += test-gstyle-parse
test_gstyle_parse_SOURCES = test-gstyle-parse.c
test_gstyle_parse_CFLAGS = $(tests_cflags)
test_gstyle_parse_LDADD = $(tests_libs)

if ENABLE_TESTS
noinst_PROGRAMS = $(TESTS) $(misc_programs)
endif

check_PROGRAMS = $(TESTS) $(misc_programs)

EXTRA_DIST +=                   \
	data/palette.gpl        \
	data/palette.gstyle.xml \
	$(NULL)

-include $(top_srcdir)/git.mk
