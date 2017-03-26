if get_option('with_gdb')

gdb_sources = [
  'gbp-gdb-debugger.c',
  'gbp-gdb-debugger.h',
  'gbp-gdb-plugin.c',
]

gdb_deps = plugin_deps + [
  libmi2_glib_dep,
]

shared_module('gdb-plugin', gdb_sources,
  dependencies: gdb_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

install_data('gdb.plugin', install_dir: plugindir)

endif
