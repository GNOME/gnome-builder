plugins_sources += files([
  'testui-plugin.c',
  'gbp-test-path.c',
  'gbp-test-output-panel.c',
  'gbp-test-tree-addin.c',
])

plugin_testui_resources = gnome.compile_resources(
  'testui-resources',
  'testui.gresource.xml',
  c_name: 'gbp_testui',
)

plugins_sources += plugin_testui_resources
