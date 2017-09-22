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
  'gdbwire.c',
  'gdbwire.h',
]

gnome_builder_plugins_sources += files(gdb_sources)
gnome_builder_plugins_sources += gdb_resources[0]

endif
