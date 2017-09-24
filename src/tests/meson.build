ide_test_env = [
  'GI_TYPELIB_PATH="@0@/src/libide:@0@/src/gstyle:$(GI_TYPELIB_PATH)"'.format(meson.build_root()),
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
  '-DTEST_DATA_DIR="@0@/data/"'.format(meson.current_source_dir()),
]

ide_test_deps = [
  libide_dep,
  libpeas_dep,
  gnome_builder_plugins_dep,
]

ide_context = executable('test-ide-context',
  'test-ide-context.c',
  c_args: ide_test_cflags,
  dependencies: [
    ide_test_deps,
  ],
)
test('test-ide-context', ide_context,
  env: ide_test_env,
)


ide_buffer_manager = executable('test-ide-buffer-manager',
  'test-ide-buffer-manager.c',
  c_args: ide_test_cflags,
  dependencies: [
    ide_test_deps,
  ],
)
test('test-ide-buffer-manager', ide_buffer_manager,
  env: ide_test_env,
)


ide_buffer = executable('test-ide-buffer',
  'test-ide-buffer.c',
  c_args: ide_test_cflags,
  dependencies: [
    ide_test_deps,
  ],
)
test('test-ide-buffer', ide_buffer,
  env: ide_test_env,
)


ide_doap = executable('test-ide-doap',
  'test-ide-doap.c',
  c_args: ide_test_cflags,
  dependencies: [
    ide_test_deps,
  ],
)
test('test-ide-doap', ide_doap,
  env: ide_test_env,
)


ide_file_settings = executable('test-ide-file-settings',
  'test-ide-file-settings.c',
  c_args: ide_test_cflags,
  dependencies: [
    ide_test_deps,
  ],
)
test('test-ide-file-settings', ide_file_settings,
  env: ide_test_env,
)


ide_indenter = executable('test-ide-indenter',
  'test-ide-indenter.c',
  c_args: ide_test_cflags,
  dependencies: [
    ide_test_deps,
  ],
)
#test('test-ide-indenter', ide_indenter,
  #env: ide_test_env,
#)


ide_vcs_uri = executable('test-ide-vcs-uri',
  'test-ide-vcs-uri.c',
  c_args: ide_test_cflags,
  dependencies: [
    ide_test_deps,
  ],
)
test('test-ide-vcs-uri', ide_vcs_uri,
  env: ide_test_env,
)


ide_uri = executable('test-ide-uri',
  'test-ide-uri.c',
  c_args: ide_test_cflags,
  dependencies: [
    ide_test_deps,
  ],
)
test('test-ide-uri', ide_uri,
  env: ide_test_env,
)


test_vim = executable('test-vim',
  'test-vim.c',
  c_args: ide_test_cflags,
  dependencies: [
    ide_test_deps,
  ],
)
#test('test-vim', test_vim,
#  env: ide_test_env,
#)


test_snippet_parser = executable('test-snippet-parser',
  'test-snippet-parser.c',
  c_args: ide_test_cflags,
  dependencies: [
    ide_test_deps,
  ],
)
#test('test-snippet-parser', test_snippet_parser,
#  env: ide_test_env,
#)

