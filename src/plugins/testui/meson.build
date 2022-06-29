plugins_sources += files([
  'testui-plugin.c',
  'gbp-test-output-panel.c',
  'gbp-testui-workspace-addin.c',
  'gbp-testui-panel.c',
])

plugin_testui_resources = gnome.compile_resources(
  'testui-resources',
  'testui.gresource.xml',
  c_name: 'gbp_testui',
)

plugins_sources += plugin_testui_resources
