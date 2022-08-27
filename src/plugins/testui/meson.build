plugins_sources += files([
  'testui-plugin.c',
  'gbp-testui-item.c',
  'gbp-testui-output-panel.c',
  'gbp-testui-panel.c',
  'gbp-testui-workspace-addin.c',
])

plugin_testui_resources = gnome.compile_resources(
  'testui-resources',
  'testui.gresource.xml',
  c_name: 'gbp_testui',
)

plugins_sources += plugin_testui_resources
