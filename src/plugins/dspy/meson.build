if get_option('plugin_dspy')

plugins_sources += files([
  'dspy-plugin.c',
  'gbp-dspy-application-addin.c',
])

plugin_dspy_resources = gnome.compile_resources(
  'dspy-resources',
  'dspy.gresource.xml',
  c_name: 'gbp_dspy',
)

plugins_sources += plugin_dspy_resources

endif
