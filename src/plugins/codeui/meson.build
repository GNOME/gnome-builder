plugins_sources += files([
  'codeui-plugin.c',
  'gbp-codeui-buffer-addin.c',
  'gbp-codeui-tree-addin.c',
])

plugin_codeui_resources = gnome.compile_resources(
  'codeui-resources',
  'codeui.gresource.xml',
  c_name: 'gbp_codeui',
)

plugins_sources += plugin_codeui_resources
