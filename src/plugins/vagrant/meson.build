if get_option('plugin_vagrant')

plugins_sources += files([
  'vagrant-plugin.c',
  'gbp-vagrant-runtime.c',
  'gbp-vagrant-runtime-provider.c',
  'gbp-vagrant-subprocess-launcher.c',
  'gbp-vagrant-table.c',
])

plugin_vagrant_resources = gnome.compile_resources(
  'vagrant-resources',
  'vagrant.gresource.xml',
  c_name: 'gbp_vagrant'
)

plugins_sources += plugin_vagrant_resources

endif
