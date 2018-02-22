if get_option('with_sysroot')

sysroot_resources = gnome.compile_resources(
  'sysroot-resources',
  'sysroot.gresource.xml',
  c_name: 'gbp_sysroot',
)

sysroot_sources = [
  'sysroot-plugin.c',
  'gbp-sysroot-manager.c',
  'gbp-sysroot-manager.h',
  'gbp-sysroot-preferences-addin.c',
  'gbp-sysroot-preferences-addin.h',
  'gbp-sysroot-preferences-row.c',
  'gbp-sysroot-preferences-row.h',
  'gbp-sysroot-runtime.c',
  'gbp-sysroot-runtime.h',
  'gbp-sysroot-runtime-provider.c',
  'gbp-sysroot-runtime-provider.h',
  'gbp-sysroot-subprocess-launcher.c',
  'gbp-sysroot-subprocess-launcher.h'
]

gnome_builder_plugins_sources += files(sysroot_sources)
gnome_builder_plugins_sources += sysroot_resources[0]

endif
