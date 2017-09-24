if get_option('with_make')

make_resources = gnome.compile_resources(
  'make-resources',
  'make.gresource.xml',
  c_name: 'gbp_make',
)

gnome_builder_plugins_sources += make_resources[0]

endif
