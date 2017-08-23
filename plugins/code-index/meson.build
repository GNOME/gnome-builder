if get_option('with_code_index')

code_index_sources = [
  'code-index-plugin.c',
  'ide-code-index-builder.c',
  'ide-code-index-builder.h',
  'ide-code-index-index.c',
  'ide-code-index-index.h',
  'ide-code-index-search-provider.c',
  'ide-code-index-search-provider.h',
  'ide-code-index-search-result.c',
  'ide-code-index-search-result.h',
  'ide-code-index-service.c',
  'ide-code-index-service.h',
  'ide-code-index-symbol-resolver.c',
  'ide-code-index-symbol-resolver.h',
  'ide-persistent-map-builder.c',
  'ide-persistent-map-builder.h',
  'ide-persistent-map.c',
  'ide-persistent-map.h',
]

shared_module('code-index-plugin', code_index_sources,
  dependencies: plugin_deps,
     link_args: plugin_link_args,
  link_depends: plugin_link_deps,
       install: true,
   install_dir: plugindir,
)

configure_file(
          input: 'code-index.plugin',
         output: 'code-index.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
