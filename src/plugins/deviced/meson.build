if get_option('plugin_deviced')

if not get_option('plugin_flatpak')
  error('-Dplugin_flatpak=true is required to enable deviced')
endif

libdeviced_dep = dependency('libdeviced', version: '>=3.27.4')

plugins_sources += files([
  'deviced-plugin.c',
  'gbp-deviced-deploy-strategy.c',
  'gbp-deviced-device.c',
  'gbp-deviced-device-provider.c',
])

plugin_deviced_resources = gnome.compile_resources(
  'deviced-resources',
  'deviced.gresource.xml',
  c_name: 'gbp_deviced',
)

plugins_deps += [libdeviced_dep]
plugins_sources += plugin_deviced_resources

gnome_builder_deviced_sources = ['gnome-builder-deviced.c']
gnome_builder_deviced_deps = [libdeviced_dep]
gnome_builder_deviced = executable('gnome-builder-deviced', gnome_builder_deviced_sources,
           install: true,
       install_dir: get_option('libexecdir'),
      dependencies: gnome_builder_deviced_deps,
)

endif
