if get_option('plugin_code_index')

plugins_sources += files([
  'code-index-plugin.c',
  'ide-code-index-builder.c',
  'ide-code-index-index.c',
  'ide-code-index-search-provider.c',
  'ide-code-index-search-result.c',
  'gbp-code-index-workbench-addin.c',
  'ide-code-index-symbol-resolver.c',
])

plugin_code_index_resources = gnome.compile_resources(
  'code-index-resources',
  'code-index.gresource.xml',
  c_name: 'gbp_code_index',
)

plugins_sources += plugin_code_index_resources[0]

endif
