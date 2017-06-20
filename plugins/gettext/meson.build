if get_option('with_gettext')

gettext_sources = [
  'ide-gettext-diagnostic-provider.c',
  'ide-gettext-diagnostic-provider.h',
  'gettext-plugin.c',
]

shared_module('gettext-plugin', gettext_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

configure_file(
          input: 'gettext.plugin',
         output: 'gettext.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
