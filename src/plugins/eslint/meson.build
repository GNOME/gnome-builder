if get_option('with_eslint')

install_data('org.gnome.builder.plugins.eslint.gschema.xml',
  install_dir: schema_dir)

eslint_resources = gnome.compile_resources(
  'eslint-resources',
  'eslint.gresource.xml',
  c_name: 'gbp_eslint',
)

gnome_builder_plugins_sources += eslint_resources[0]

endif
