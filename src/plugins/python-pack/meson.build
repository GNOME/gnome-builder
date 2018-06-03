if get_option('with_python_pack')

python_pack_resources = gnome.compile_resources(
  'python-pack-resources',
  'python-pack.gresource.xml',
  c_name: 'ide_python_pack',
)

python_pack_sources = [
  'ide-python-indenter.c',
  'ide-python-indenter.h',
  'python-pack-plugin.c',
]

gnome_builder_plugins_sources += files(python_pack_sources)
gnome_builder_plugins_sources += python_pack_resources[0]

endif
