template_libdir = dependency('template-glib-1.0').get_pkgconfig_variable('libdir')
dazzle_libdir = dependency('libdazzle-1.0').get_pkgconfig_variable('libdir')
gsv_libdir = dependency('gtksourceview-3.0').get_pkgconfig_variable('libdir')

typelib_dirs = [
  '@0@/src/libide'.format(meson.build_root()),
  '@0@/src/gstyle'.format(meson.build_root()),
  join_paths(template_libdir, 'girepository-1.0'),
  join_paths(dazzle_libdir, 'girepository-1.0'),
  join_paths(gsv_libdir, 'girepository-1.0'),
]

ide_test_env = [
  'GI_TYPELIB_PATH="@0@:$(GI_TYPELIB_PATH)"'.format(':'.join(typelib_dirs)),
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'G_DEBUG=gc-friendly',
  'GSETTINGS_BACKEND=memory',
  'GSETTINGS_SCHEMA_DIR=@0@/data/gsettings:$(GSETTINGS_SCHEMA_DIR)'.format(meson.build_root()),
  'PYTHONDONTWRITEBYTECODE=yes',
  'MALLOC_CHECK_=2',
#  'MALLOC_PERTURB_=$((${RANDOM:-256} % 256))',
]
ide_test_cflags = [
  '-DTEST_DATA_DIR="@0@/data/"'.format(meson.current_source_dir()),
  '-I' + join_paths(meson.source_root(), 'src'),
]

ide_test_deps = [
  libide_dep,
  libpeas_dep,
  gnome_builder_plugins_dep,
]


ide_compile_commands = executable('test-ide-compile-commands', 'test-ide-compile-commands.c',
        c_args: ide_test_cflags,
  dependencies: [ ide_test_deps ],
)
test('test-ide-compile-commands', ide_compile_commands, env: ide_test_env)


ide_context = executable('test-ide-context', 'test-ide-context.c',
        c_args: ide_test_cflags,
  dependencies: [ide_test_deps]
)
test('test-ide-context', ide_context, env: ide_test_env)


ide_runtime = executable('test-ide-runtime', 'test-ide-runtime.c',
        c_args: ide_test_cflags,
  dependencies: [ide_test_deps]
)
test('test-ide-runtime', ide_runtime, env: ide_test_env)


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


test_ide_glib = executable('test-ide-glib', 'test-ide-glib.c',
  c_args: ide_test_cflags,
  dependencies: [ ide_test_deps ],
)
test('test-ide-glib', test_ide_glib, env: ide_test_env)


test_line_reader = executable('test-line-reader', 'test-line-reader.c',
  c_args: ide_test_cflags,
  dependencies: [ ide_test_deps ],
)
test('test-line-reader', test_line_reader, env: ide_test_env)


test_ide_task = executable('test-ide-task', 'test-ide-task.c',
  c_args: ide_test_cflags,
  dependencies: [ ide_test_deps ],
)
test('test-ide-task', test_ide_task, env: ide_test_env)


test_iter = executable('test-iter', 'test-iter.c',
  c_args: ide_test_cflags,
  dependencies: [ ide_test_deps ],
)
test('test-iter', test_iter, env: ide_test_env)


test_hdr_format = executable('test-hdr-format', [
  'test-hdr-format.c',
  '../plugins/c-pack/c-parse-helper.c',
],
        c_args: ide_test_cflags,
  dependencies: [ ide_test_deps ],
)
