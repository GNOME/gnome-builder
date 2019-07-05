plugins_sources += files([
  'docsui-plugin.c',
  'gbp-docsui-application-addin.c',
])

plugin_docsui_resources = gnome.compile_resources(
  'docsui-resources',
  'docsui.gresource.xml',
  c_name: 'gbp_docsui',
)

plugins_sources += plugin_docsui_resources
