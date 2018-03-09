if get_option('with_qemu')

qemu_resources = gnome.compile_resources(
  'qemu-resources',
  'qemu.gresource.xml',
  c_name: 'gbp_qemu',
)

qemu_sources = [
  'gbp-qemu-plugin.c',
  'gbp-qemu-device-provider.c',
]

gnome_builder_plugins_sources += files(qemu_sources)
gnome_builder_plugins_sources += qemu_resources[0]

endif
