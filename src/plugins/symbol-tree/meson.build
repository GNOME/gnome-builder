if get_option('with_symbol_tree')

symbol_tree_resources = gnome.compile_resources(
  'symbol-tree-resources',
  'symbol-tree.gresource.xml',
  c_name: 'symbol_tree',
)

symbol_tree_sources = [
  symbol_tree_resources[0],
  symbol_tree_resources[1],
  'gbp-symbol-layout-stack-addin.c',
  'gbp-symbol-layout-stack-addin.h',
  'gbp-symbol-menu-button.c',
  'gbp-symbol-menu-button.h',
  'gbp-symbol-tree-builder.c',
  'gbp-symbol-tree-builder.h',
  'symbol-tree-plugin.c',
]

shared_module('symbol-tree-plugin', symbol_tree_sources,
  dependencies: plugin_deps,
     link_args: plugin_link_args,
  link_depends: plugin_link_deps,
       install: true,
   install_dir: plugindir,
)

configure_file(
          input: 'symbol-tree.plugin',
         output: 'symbol-tree.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
