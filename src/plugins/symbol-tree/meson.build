plugins_sources += files([
  'gbp-symbol-hover-provider.c',
  'gbp-symbol-list-model.c',
  'gbp-symbol-search-provider.c',
  'gbp-symbol-search-result.c',
  'gbp-symbol-popover.c',
  'gbp-symbol-util.c',
  'gbp-symbol-workspace-addin.c',
  'symbol-tree-plugin.c',
])

plugin_symbol_tree_resources = gnome.compile_resources(
  'gbp-symbol-tree-resources',
  'symbol-tree.gresource.xml',
  c_name: 'gbp_symbol_tree',
)

plugins_sources += plugin_symbol_tree_resources
