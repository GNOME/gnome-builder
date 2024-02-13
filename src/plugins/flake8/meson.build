if get_option('plugin_flake8')

plugins_sources += files([
  'flake8-plugin.c',
  'gbp-flake8-diagnostic-provider.c',
])

plugin_flake8_resources = gnome.compile_resources(
  'flake8-resources',
  'flake8.gresource.xml',
  c_name: 'gbp_flake8',
)

plugins_sources += plugin_flake8_resources

endif
