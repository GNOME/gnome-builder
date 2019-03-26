if get_option('plugin_code_index')

plugins_sources += files([
  'code-index-plugin.c',
  'gbp-code-index-application-addin.c',
  'gbp-code-index-builder.c',
  'gbp-code-index-executor.c',
  'gbp-code-index-plan.c',
  'gbp-code-index-service.c',
  'gbp-code-index-workbench-addin.c',
  'ide-code-index-index.c',
  'ide-code-index-search-provider.c',
  'ide-code-index-search-result.c',
  'ide-code-index-symbol-resolver.c',
  'indexer-info.c',
])

plugin_code_index_resources = gnome.compile_resources(
  'code-index-resources',
  'code-index.gresource.xml',
  c_name: 'gbp_code_index',
)

plugins_sources += plugin_code_index_resources

endif
