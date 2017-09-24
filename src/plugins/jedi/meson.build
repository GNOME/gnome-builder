if get_option('with_jedi')

jedi_resources = gnome.compile_resources(
  'jedi-resources',
  'jedi.gresource.xml',
  c_name: 'gbp_jedi',
)

gnome_builder_plugins_sources += jedi_resources[0]

endif
