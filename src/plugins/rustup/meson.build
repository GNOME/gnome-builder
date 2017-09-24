if get_option('with_rustup')

rustup_resources = gnome.compile_resources(
  'rustup_plugin',
  'rustup.gresource.xml',
  c_name: 'gbp_rustup',
)

gnome_builder_plugins_sources += rustup_resources[0]

endif
