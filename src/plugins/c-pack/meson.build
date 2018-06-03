if get_option('with_c_pack')

c_pack_resources = gnome.compile_resources(
  'c-pack-resources',
  'c-pack.gresource.xml',
  c_name: 'ide_c',
)

c_pack_sources = [
  'c-pack-plugin.c',
  'c-parse-helper.c',
  'c-parse-helper.h',
  'ide-c-indenter.c',
  'ide-c-indenter.h',
]

gnome_builder_plugins_sources += files(c_pack_sources)
gnome_builder_plugins_sources += c_pack_resources[0]

endif
