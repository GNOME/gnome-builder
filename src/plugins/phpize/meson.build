if get_option('plugin_phpize')

plugins_sources += files([
  'phpize-plugin.c',
  'gbp-phpize-build-system-discovery.c',
  'gbp-phpize-build-system.c',
  'gbp-phpize-pipeline-addin.c',
])

plugin_phpize_resources = gnome.compile_resources(
  'phpize-resources',
  'phpize.gresource.xml',
  c_name: 'gbp_phpize',
)

plugins_sources += plugin_phpize_resources

endif
