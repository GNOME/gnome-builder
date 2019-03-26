plugins_sources += files([
  'gbp-symbol-hover-provider.c',
  'gbp-symbol-frame-addin.c',
  'gbp-symbol-menu-button.c',
  'gbp-symbol-tree-builder.c',
  'symbol-tree-plugin.c',
])

plugin_symbol_tree_resources = gnome.compile_resources(
  'gbp-symbol-tree-resources',
  'symbol-tree.gresource.xml',
  c_name: 'gbp_symbol_tree',
)

plugins_sources += plugin_symbol_tree_resources
