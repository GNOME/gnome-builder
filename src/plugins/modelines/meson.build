if get_option('plugin_modelines')

plugins_sources += files([
  'gbp-modelines-file-settings.c',
  'modeline-parser.c',
  'modelines-plugin.c',
])

plugin_modelines_resources = gnome.compile_resources(
  'gbp-modelines-resources',
  'modelines.gresource.xml',
  c_name: 'gbp_modelines',
)

plugins_sources += plugin_modelines_resources

endif
