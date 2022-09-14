plugins_sources += files([
  'sessionui-plugin.c',
  'gbp-sessionui-editor-page-addin.c',
  'gbp-sessionui-workbench-addin.c',
])

plugin_sessionui_resources = gnome.compile_resources(
  'sessionui-resources',
  'sessionui.gresource.xml',
  c_name: 'gbp_sessionui',
)

plugins_sources += plugin_sessionui_resources
