gstyle_test_env = [
  'GB_IN_TREE_PLUGINS=1',
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'G_DEBUG=gc-friendly',
  'GSETTINGS_BACKEND=memory',
  'MALLOC_CHECK_=2',
#  'MALLOC_PERTURB_=$((${RANDOM:-256} % 256))',
]
gstyle_test_cflags = [
  '-DTEST_DATA_DIR="@0@/data"'.format(meson.current_source_dir()),
]

gstyle_color = executable('test-gstyle-color',
  'test-gstyle-color.c',
  dependencies: libgstyle_dep,
)
test('test-gstyle-color', gstyle_color,
  env: gstyle_test_env,
  timeout: 650
)


gstyle_color_panel = executable('test-gstyle-color-panel',
  'test-gstyle-color-panel.c',
  c_args: gstyle_test_cflags,
  dependencies: libgstyle_dep,
)
#test('test-gstyle-color-panel', gstyle_color_panel,
  #env: gstyle_test_env
#)


gstyle_color_plane = executable('test-gstyle-color-plane',
  'test-gstyle-color-plane.c',
  c_args: gstyle_test_cflags,
  dependencies: libgstyle_dep,
)
#test('test-gstyle-color-plane', gstyle_color_plane,
#  env: gstyle_test_env
#)


gstyle_color_scale = executable('test-gstyle-color-scale',
  'test-gstyle-color-scale.c',
  c_args: gstyle_test_cflags,
  dependencies: libgstyle_dep,
)
#test('test-gstyle-color-scale', gstyle_color_scale,
#  env: gstyle_test_env
#)


gstyle_color_widget = executable('test-gstyle-color-widget',
  'test-gstyle-color-widget.c',
  c_args: gstyle_test_cflags,
  dependencies: libgstyle_dep,
)
#test('test-gstyle-color-widget', gstyle_color_widget,
#  env: gstyle_test_env
#)


gstyle_filter = executable('test-gstyle-filter',
  'test-gstyle-filter.c',
  c_args: gstyle_test_cflags,
  dependencies: libgstyle_dep,
)
#test('test-gstyle-filter', gstyle_filter,
#  env: gstyle_test_env
#)


gstyle_palette = executable('test-gstyle-palette',
  'test-gstyle-palette.c',
  c_args: gstyle_test_cflags,
  dependencies: libgstyle_dep,
)
test('test-gstyle-palette', gstyle_palette,
  env: gstyle_test_env
)


gstyle_palette_widget = executable('test-gstyle-palette-widget',
  'test-gstyle-palette-widget.c',
  c_args: gstyle_test_cflags,
  dependencies: libgstyle_dep,
)
#test('test-gstyle-color-palette-widget', gstyle_palette_widget,
  #env: gstyle_test_env
#)


gstyle_parse = executable('test-gstyle-color-parse',
  'test-gstyle-parse.c',
  c_args: gstyle_test_cflags,
  dependencies: libgstyle_dep,
)
test('test-gstyle-parse', gstyle_parse,
  env: gstyle_test_env
)
