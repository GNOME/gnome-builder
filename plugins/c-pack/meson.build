if get_option('with_c_pack')

c_pack_sources = [
  'c-pack-plugin.c',
  'c-parse-helper.c',
  'c-parse-helper.h',
  'ide-c-indenter.c',
  'ide-c-indenter.h',
  'ide-c-format-provider.c',
  'ide-c-format-provider.h',
]

shared_module('c-pack-plugin', c_pack_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

configure_file(
          input: 'c-pack.plugin',
         output: 'c-pack.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
