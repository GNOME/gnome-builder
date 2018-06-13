if get_option('with_c_pack')

c_pack_resources = gnome.compile_resources(
  'c-pack-resources',
  'c-pack.gresource.xml',
  c_name: 'ide_c',
)

c_pack_sources = [
  'c-pack-plugin.c',
  'c-parse-helper.c',
  'hdr-format.c',
  'ide-c-indenter.c',
  'cpack-editor-view-addin.c',
]

gnome_builder_plugins_sources += files(c_pack_sources)
gnome_builder_plugins_sources += c_pack_resources[0]

endif
