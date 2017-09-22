if get_option('with_file_search')

file_search_resources = gnome.compile_resources(
  'file-search-resources',
  'file-search.gresource.xml',
  c_name: 'gb_file_search',
)

file_search_sources = [
  'gb-file-search-provider.c',
  'gb-file-search-provider.h',
  'gb-file-search-result.c',
  'gb-file-search-result.h',
  'gb-file-search-index.c',
  'gb-file-search-index.h',
]

gnome_builder_plugins_sources += files(file_search_sources)
gnome_builder_plugins_sources += file_search_resources[0]

endif
