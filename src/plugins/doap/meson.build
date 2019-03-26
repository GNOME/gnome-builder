plugins_sources += files([
  'doap-plugin.c',
  'gbp-doap-workbench-addin.c',
])

plugin_doap_resources = gnome.compile_resources(
  'doap-resources',
  'doap.gresource.xml',
  c_name: 'gbp_doap',
)

plugins_sources += plugin_doap_resources
