if get_option('plugin_rstcheck')

plugins_sources += files([
  'rstcheck-plugin.c',
  'gbp-rstcheck-diagnostic-provider.c',
])

plugin_rstcheck_resources = gnome.compile_resources(
  'rstcheck-resources',
  'rstcheck.gresource.xml',
  c_name: 'gbp_rstcheck',
)

plugins_sources += plugin_rstcheck_resources

endif
