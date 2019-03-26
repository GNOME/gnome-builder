if get_option('plugin_gdb')

plugins_sources += files([
  'gbp-gdb-debugger.c',
  'gdb-plugin.c',
])

plugin_gdb_resources = gnome.compile_resources(
  'gdb-resources',
  'gdb.gresource.xml',
  c_name: 'gbp_gdb',
)

plugins_sources += plugin_gdb_resources

gdbwire = static_library('gdbwire', ['gdbwire.c'],
  c_args: [ '-Wno-redundant-decls',
            '-Wno-switch-default',
            '-Wno-missing-declarations',
            '-Wno-uninitialized',
            '-Wno-declaration-after-statement' ],
)

plugins_link_with += gdbwire

endif
