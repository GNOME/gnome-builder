if get_option('plugin_valac')

plugins_sources += files([
  'valac-plugin.c',
  'gbp-valac-pipeline-addin.c',
])

plugin_valac_resources = gnome.compile_resources(
  'valac-resources',
  'valac.gresource.xml',
  c_name: 'gbp_valac',
)

plugins_sources += plugin_valac_resources

endif
