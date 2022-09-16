plugins_sources += files([
  'gbp-ls-model.c',
  'gbp-ls-page.c',
  'gbp-ls-editor-page-addin.c',
  'gbp-ls-workbench-addin.c',
  'gbp-ls-workspace-addin.c',
  'gbp-ls-tree-view.c',
  'ls-plugin.c',
])

plugin_ls_resources = gnome.compile_resources(
  'ls-resources',
  'ls.gresource.xml',
  c_name: 'gbp_ls',
)

plugins_sources += plugin_ls_resources
