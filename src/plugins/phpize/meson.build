if get_option('with_phpize')

phpize_resources = gnome.compile_resources(
  'phpize-resources',
  'phpize.gresource.xml',
  c_name: 'gbp_phpize',
)

gnome_builder_plugins_sources += phpize_resources[0]

endif
