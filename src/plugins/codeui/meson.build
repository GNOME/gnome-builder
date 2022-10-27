plugins_sources += files([
  'codeui-plugin.c',
  'gbp-codeui-buffer-addin.c',
  'gbp-codeui-code-action-dialog.c',
  'gbp-codeui-editor-page-addin.c',
  'gbp-codeui-hover-provider.c',
  'gbp-codeui-range-dialog.c',
  'gbp-codeui-rename-dialog.c',
  'gbp-codeui-tree-addin.c',
])

plugin_codeui_resources = gnome.compile_resources(
  'codeui-resources',
  'codeui.gresource.xml',
  c_name: 'gbp_codeui',
)

plugins_sources += plugin_codeui_resources
