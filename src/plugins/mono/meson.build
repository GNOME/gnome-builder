if get_option('plugin_mono')

plugins_sources += files([
  'mono-plugin.c',
  'gbp-mono-pipeline-addin.c',
])

plugin_mono_resources = gnome.compile_resources(
  'mono-resources',
  'mono.gresource.xml',
  c_name: 'gbp_mono',
)

plugins_sources += plugin_mono_resources

endif
