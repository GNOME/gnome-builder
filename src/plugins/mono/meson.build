if get_option('with_mono')

mono_resources = gnome.compile_resources(
  'mono-resources',
  'mono.gresource.xml',
  c_name: 'gbp_mono',
)

gnome_builder_plugins_sources += mono_resources[0]

endif
