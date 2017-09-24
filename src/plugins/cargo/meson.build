if get_option('with_cargo')

cargo_resources = gnome.compile_resources(
  'cargo-resources',
  'cargo.gresource.xml',
  c_name: 'gbp_cargo',
)

gnome_builder_plugins_sources += cargo_resources[0]

endif
