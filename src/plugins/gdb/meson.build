if get_option('with_gdb')

gdb_resources = gnome.compile_resources(
  'gdb-resources',
  'gdb.gresource.xml',
  c_name: 'gbp_gdb',
)

gdb_sources = [
  'gbp-gdb-debugger.c',
  'gbp-gdb-debugger.h',
  'gbp-gdb-plugin.c',
]

gdbwire = static_library('gdbwire', ['gdbwire.c'],
  c_args: [ '-Wno-redundant-decls',
            '-Wno-switch-default',
            '-Wno-missing-declarations',
            '-Wno-uninitialized',
            '-Wno-declaration-after-statement' ],
)

gnome_builder_plugins_sources += files(gdb_sources)
gnome_builder_plugins_sources += gdb_resources[0]
gnome_builder_plugins_link_with += gdbwire

endif
