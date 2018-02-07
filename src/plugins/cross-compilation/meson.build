if get_option('with_cross_compilation')

cross_compilation_resources = gnome.compile_resources(
  'cross-compilation-resources',
  'cross-compilation.gresource.xml',
  c_name: 'ide_cross_compilation',
)

cross_compilation_sources = [
  'cross-compilation-plugin.c',
  'ide-cross-compilation-device.c',
  'ide-cross-compilation-device-provider.c',
  'ide-cross-compilation-preferences-addin.c'
]

gnome_builder_plugins_sources += files(cross_compilation_sources)
gnome_builder_plugins_sources += cross_compilation_resources[0]

endif
