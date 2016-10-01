if get_option('with_symbol_tree')

symbol_tree_resources = gnome.compile_resources(
  'symbol-tree-resources',
  'symbol-tree.gresource.xml',
  c_name: 'symbol_tree',
)

symbol_tree_sources = [
  symbol_tree_resources[0],
  symbol_tree_resources[1],
  'symbol-tree-builder.c',
  'symbol-tree-builder.h',
  'symbol-tree-panel.c',
  'symbol-tree-panel.h',
  'symbol-tree.c',
  'symbol-tree.h',
]

shared_module('symbol-tree', symbol_tree_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

install_data('symbol-tree.plugin', install_dir: plugindir)

endif
