if get_option('with_symbol_tree')

symbol_tree_resources = gnome.compile_resources(
  'symbol-tree-resources',
  'symbol-tree.gresource.xml',
  c_name: 'symbol_tree',
)

symbol_tree_sources = [
  'gbp-symbol-hover-provider.c',
  'gbp-symbol-layout-stack-addin.c',
  'gbp-symbol-layout-stack-addin.h',
  'gbp-symbol-menu-button.c',
  'gbp-symbol-menu-button.h',
  'gbp-symbol-tree-builder.c',
  'gbp-symbol-tree-builder.h',
  'symbol-tree-plugin.c',
]

gnome_builder_plugins_sources += files(symbol_tree_sources)
gnome_builder_plugins_sources += symbol_tree_resources[0]

endif
