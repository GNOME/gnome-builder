gi_typelib_path = [
  '@0@/src'.format(meson.project_build_root()),
]

typelib_deps = [
  libadwaita_dep,
  libgio_dep,
  libgtk_dep,
  libgtksource_dep,
  libjson_glib_dep,
  libjsonrpc_glib_dep,
  libpanel_dep,
  libpeas_dep,
  libtemplate_glib_dep,
  libvte_dep,
]

foreach typelib_dep: typelib_deps
  gi_typelib_path += [join_paths(typelib_dep.get_variable('libdir'), 'girepository-1.0')]
endforeach

test_env = [
  'GI_TYPELIB_PATH=@0@'.format(':'.join(gi_typelib_path)),
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'G_DEBUG=gc-friendly',
  'GSETTINGS_BACKEND=memory',
  'GSETTINGS_SCHEMA_DIR=@0@/data/gsettings'.format(meson.project_build_root()),
  'PYTHONDONTWRITEBYTECODE=yes',
  'MALLOC_CHECK_=2',
  'NO_AT_BRIDGE=1',
]

test_cflags = [
  '-DTEST_DATA_DIR="@0@/data/"'.format(meson.current_source_dir()),
  '-I' + join_paths(meson.project_source_root(), 'src'),
]


test_libide_core = executable('test-libide-core', 'test-libide-core.c',
        c_args: test_cflags,
  dependencies: [ libide_core_dep ],
)
test('test-libide-core', test_libide_core, env: test_env)


test_libide_io = executable('test-libide-io', 'test-libide-io.c',
        c_args: test_cflags,
  dependencies: [ libide_io_dep ],
)
test('test-libide-io', test_libide_io, env: test_env)


test_line_reader = executable('test-line-reader', 'test-line-reader.c',
        c_args: test_cflags,
  dependencies: [ libide_io_dep ],
)
test('test-line-reader', test_line_reader, env: test_env)


test_text_iter = executable('test-text-iter', 'test-text-iter.c',
        c_args: test_cflags,
  dependencies: [ libide_sourceview_dep ],
)
test('test-text-iter', test_text_iter, env: test_env)


test_vcs_uri = executable('test-vcs-uri', 'test-vcs-uri.c',
        c_args: test_cflags,
  dependencies: [ libide_vcs_dep ],
)
test('test-vcs-uri', test_vcs_uri, env: test_env)


test_task = executable('test-task', 'test-task.c',
        c_args: test_cflags,
  dependencies: [ libide_threading_dep ],
)
test('test-task', test_task, env: test_env)


test_subprocess_launcher = executable('test-subprocess-launcher', 'test-subprocess-launcher.c',
        c_args: test_cflags,
  dependencies: [ libide_threading_dep ],
)
test('test-subprocess-launcher', test_subprocess_launcher, env: test_env)


test_gfile = executable('test-gfile', 'test-gfile.c',
        c_args: test_cflags,
  dependencies: [ libide_io_dep ],
)
test('test-gfile', test_gfile, env: test_env)


test_doap = executable('test-doap', 'test-doap.c',
        c_args: test_cflags,
  dependencies: [ libide_projects_dep ],
)
test('test-doap', test_doap, env: test_env)


test_compile_commands = executable('test-compile-commands', 'test-compile-commands.c',
        c_args: test_cflags,
  dependencies: [ libide_foundry_dep ],
)
test('test-compile-commands', test_compile_commands, env: test_env)

test_shortcuts = executable('test-shortcuts', 'test-shortcuts.c',
        c_args: test_cflags,
  dependencies: [ libgtk_dep, libide_gui_dep ],
)
test('test-shortcuts', test_shortcuts, env: test_env)
