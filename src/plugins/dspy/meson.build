if get_option('plugin_dspy')

plugins_sources += files([
  'dspy-plugin.c',
  'dspy-connection-model.c',
  'gbp-dspy-workspace-addin.c',
])

plugin_dspy_resources = gnome.compile_resources(
  'dspy-resources',
  'dspy.gresource.xml',
  c_name: 'gbp_dspy',
)

plugins_sources += plugin_dspy_resources

endif
