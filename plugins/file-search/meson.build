if get_option('with_file_search')

file_search_sources = [
  'gb-file-search-provider.c',
  'gb-file-search-provider.h',
  'gb-file-search-result.c',
  'gb-file-search-result.h',
  'gb-file-search-index.c',
  'gb-file-search-index.h',
]

shared_module('file-search', file_search_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

configure_file(
          input: 'file-search.plugin',
         output: 'file-search.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
