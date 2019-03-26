if get_option('plugin_qemu')

plugins_sources += files([
  'qemu-plugin.c',
  'gbp-qemu-device-provider.c',
])

plugin_qemu_resources = gnome.compile_resources(
  'qemu-resources',
  'qemu.gresource.xml',
  c_name: 'gbp_qemu',
)

plugins_sources += plugin_qemu_resources

endif
