if get_option('with_jhbuild')

jhbuild_resources = gnome.compile_resources(
  'jhbuild-resources',
  'jhbuild.gresource.xml',
  c_name: 'gbp_jhbuild',
)

gnome_builder_plugins_sources += jhbuild_resources[0]

endif
