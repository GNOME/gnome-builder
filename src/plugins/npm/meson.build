if get_option('with_npm')

npm_resources = gnome.compile_resources(
  'npm-resources',
  'npm.gresource.xml',
  c_name: 'gbp_npm',
)

gnome_builder_plugins_sources += npm_resources[0]

endif
