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
  dependencies: [ libide_threading_dep, libide_io_dep ],
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
test('test-shortcuts', test_shortcuts, env: test_env, depends: [libide_gir])

test_run_context = executable('test-run-context', 'test-run-context.c',
        c_args: test_cflags,
  dependencies: [ libide_foundry_dep ],
)
test('test-run-context', test_run_context, env: test_env)
