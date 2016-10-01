ide_test_env = [
  'GI_TYPELIB_PATH="@0@/libide:@0@/contrib/tmpl:@0@/contrib/egg:@0@/contrib/pnl:$(GI_TYPELIB_PATH)"'.format(meson.build_root()),
  'GB_IN_TREE_PLUGINS=1',
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'G_DEBUG=gc-friendly',
  'GSETTINGS_BACKEND=memory',
  'PYTHONDONTWRITEBYTECODE=yes',
  'MALLOC_CHECK_=2',
#  'MALLOC_PERTURB_=$((${RANDOM:-256} % 256))',
]
ide_test_cflags = [
	'-DTEST_DATA_DIR="@0@/data"'.format(meson.current_source_dir()),
]

ide_context = executable('test-ide-context',
  'test-ide-context.c',
  c_args: ide_test_cflags,
  dependencies: libide_dep,
)
test('test-ide-context', ide_context,
  env: ide_test_env,
)


ide_back_forward_list = executable('test-ide-back-forward-list',
  'test-ide-back-forward-list.c',
  c_args: ide_test_cflags,
  dependencies: libide_dep,
)
test('test-ide-back-forward-list', ide_back_forward_list,
  env: ide_test_env,
)


ide_buffer_manager = executable('test-ide-buffer-manager',
  'test-ide-buffer-manager.c',
  c_args: ide_test_cflags,
  dependencies: libide_dep,
)
test('test-ide-buffer-manager', ide_buffer_manager,
  env: ide_test_env,
)


ide_buffer = executable('test-ide-buffer',
  'test-ide-buffer.c',
  c_args: ide_test_cflags,
  dependencies: libide_dep,
)
test('test-ide-buffer', ide_buffer,
  env: ide_test_env,
)


ide_doap = executable('test-ide-doap',
  'test-ide-doap.c',
  c_args: ide_test_cflags,
  dependencies: libide_dep,
)
test('test-ide-doap', ide_doap,
  env: ide_test_env,
)


ide_file_settings = executable('test-ide-file-settings',
  'test-ide-file-settings.c',
  c_args: ide_test_cflags,
  dependencies: libide_dep,
)
test('test-ide-file-settings', ide_file_settings,
  env: ide_test_env,
)


ide_indenter = executable('test-ide-indenter',
  'test-ide-indenter.c',
  c_args: ide_test_cflags,
  dependencies: [
    libide_dep,
    libpeas_dep,
  ],
)
test('test-ide-indenter', ide_indenter,
  env: ide_test_env,
)


ide_vcs_uri = executable('test-ide-vcs-uri',
  'test-ide-vcs-uri.c',
  c_args: ide_test_cflags,
  dependencies: libide_dep,
)
test('test-ide-vcs-uri', ide_vcs_uri,
  env: ide_test_env,
)


ide_uri = executable('test-ide-uri',
  'test-ide-uri.c',
  c_args: ide_test_cflags,
  dependencies: libide_dep,
)
test('test-ide-uri', ide_uri,
  env: ide_test_env,
)


test_vim = executable('test-vim',
  'test-vim.c',
  c_args: ide_test_cflags,
  dependencies: libide_dep,
)
test('test-vim', test_vim,
  env: ide_test_env,
)


test_cpu_graph = executable('test-cpu-graph',
  'test-cpu-graph.c',
  c_args: ide_test_cflags,
  dependencies: [libide_dep, librg_dep],
)
#test('test-cpu-graph', test_cpu_graph,
#  env: ide_test_env,
#)


test_fuzzy = executable('test-fuzzy',
  'test-fuzzy.c',
  c_args: ide_test_cflags,
  dependencies: [libide_dep, libsearch_dep],
)
#test('test-fuzzy', test_fuzzy,
#  env: ide_test_env,
#)


test_egg_slider = executable('test-egg-slider',
  'test-egg-slider.c',
  c_args: ide_test_cflags,
  dependencies: libegg_dep,
)
#test('test-egg-slider', test_egg_slider,
#  env: ide_test_env,
#)


test_snippet_parser = executable('test-snippet-parser',
  'test-snippet-parser.c',
  c_args: ide_test_cflags,
  dependencies: libide_dep,
)
#test('test-snippet-parser', test_snippet_parser,
#  env: ide_test_env,
#)


test_egg_binding_group = executable('test-egg-binding-group',
  'test-egg-binding-group.c',
  c_args: ide_test_cflags,
  dependencies: libegg_dep,
)
test('test-egg-binding-group', test_egg_binding_group,
  env: ide_test_env,
)


test_egg_menu_manager = executable('test-egg-menu-manager',
  'test-egg-menu-manager.c',
  c_args: ide_test_cflags,
  dependencies: libegg_dep,
)
#test('test-egg-menu-manager', test_egg_menu_manager,
#  env: ide_test_env,
#)


test_egg_state_machine = executable('test-egg-state-machine',
  'test-egg-state-machine.c',
  c_args: ide_test_cflags,
  dependencies: libegg_dep,
)
test('test-egg-state-machine', test_egg_state_machine,
  env: ide_test_env,
)


test_egg_radio_box = executable('test-egg-radio-box',
  'test-egg-radio-box.c',
  c_args: ide_test_cflags,
  dependencies: libegg_dep,
)
#test('test-egg-radio-box', test_egg_radio_box,
#  env: ide_test_env,
#)


test_egg_file_chooser_entry = executable('test-egg-file-chooser-entry',
  'test-egg-file-chooser-entry.c',
  c_args: ide_test_cflags,
  dependencies: libegg_dep,
)
#test('test-egg-file-chooser-entry', test_egg_file_chooser_entry,
#  env: ide_test_env,
#)


test_egg_cache = executable('test-egg-cache',
  'test-egg-cache.c',
  c_args: ide_test_cflags,
  dependencies: libegg_dep,
)
test('test-egg-cache', test_egg_cache,
  env: ide_test_env,
)


test_egg_heap = executable('test-egg-heap',
  'test-egg-heap.c',
  c_args: ide_test_cflags,
  dependencies: libegg_dep,
)
test('test-egg-heap', test_egg_heap,
  env: ide_test_env,
)

