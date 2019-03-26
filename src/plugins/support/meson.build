plugins_sources += files([
  'ide-support-application-addin.c',
  'ide-support.c',
  'support-plugin.c',
])

plugin_support_resources = gnome.compile_resources(
  'support-resources',
  'support.gresource.xml',
  c_name: 'gbp_support',
)

plugins_sources += plugin_support_resources
