if get_option('plugin_gjs_symbols')

plugins_sources += files([
  'gjs-symbols-plugin.c',
  'gbp-gjs-symbol-resolver.c',
  'gbp-gjs-symbol-node.c',
  'gbp-gjs-symbol-tree.c',
  'gbp-gjs-code-indexer.c',
])

plugin_gjs_symbols_resources = gnome.compile_resources(
  'gjs-symbols-resources',
  'gjs-symbols.gresource.xml',
  c_name: 'gbp_gjs_symbols',
)

plugins_sources += plugin_gjs_symbols_resources

endif
