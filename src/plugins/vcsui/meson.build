plugins_sources += files([
  'vcsui-plugin.c',
  'gbp-vcsui-tree-addin.c',
  'gbp-vcsui-editor-page-addin.c',
  'gbp-vcsui-workbench-addin.c',
])

plugin_vcsui_resources = gnome.compile_resources(
  'vcsui-resources',
  'vcsui.gresource.xml',
  c_name: 'gbp_vcsui',
)

plugins_sources += plugin_vcsui_resources
