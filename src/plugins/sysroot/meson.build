if get_option('with_sysroot')

sysroot_resources = gnome.compile_resources(
  'sysroot-resources',
  'sysroot.gresource.xml',
  c_name: 'ide_sysroot',
)

sysroot_sources = [
  'sysroot-plugin.c',
  'ide-sysroot-runtime.c',
  'ide-sysroot-runtime.h',
  'ide-sysroot-runtime-provider.c',
  'ide-sysroot-runtime-provider.h',
  'ide-host-subprocess-launcher.c',
  'ide-host-subprocess-launcher.h',
  'ide-sysroot-preferences-addin.c',
  'ide-sysroot-preferences-addin.h',
  'ide-sysroot-preferences-row.c',
  'ide-sysroot-preferences-row.h',
  'ide-sysroot-manager.c',
  'ide-sysroot-manager.h'
]

gnome_builder_plugins_sources += files(sysroot_sources)
gnome_builder_plugins_sources += sysroot_resources[0]

endif
