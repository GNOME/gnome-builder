plugins_sources += files([
  'greeter-plugin.c',
  'gbp-greeter-application-addin.c',
])

plugin_greeter_resources = gnome.compile_resources(
  'gbp-greeter-resources',
  'greeter.gresource.xml',
  c_name: 'gbp_greeter',
)

plugins_sources += plugin_greeter_resources
