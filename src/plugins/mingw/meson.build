if get_option('with_mingw')

mingw_resources = gnome.compile_resources(
  'mingw-resources',
  'mingw.gresource.xml',
  c_name: 'ide_mingw',
)

mingw_sources = [
  'mingw-plugin.c',
  'ide-mingw-device.c',
  'ide-mingw-device.h',
  'ide-mingw-device-provider.c',
  'ide-mingw-device-provider.h',
]

gnome_builder_plugins_sources += files(mingw_sources)
gnome_builder_plugins_sources += mingw_resources[0]

endif
