if get_option('plugin_file_search')

plugins_sources += files([
  'gbp-file-search-provider.c',
  'gbp-file-search-result.c',
  'gbp-file-search-index.c',
  'file-search-plugin.c',
])

plugin_file_search_resources = gnome.compile_resources(
  'file-search-resources',
  'file-search.gresource.xml',
  c_name: 'gbp_file_search',
)

plugins_sources += plugin_file_search_resources

endif
