if get_option('with_mingw')

mingw_sources = [
  'mingw-plugin.c',
  'ide-mingw-device.c',
  'ide-mingw-device.h',
  'ide-mingw-device-provider.c',
  'ide-mingw-device-provider.h',
]

shared_module('mingw-plugin', mingw_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

configure_file(
          input: 'mingw.plugin',
         output: 'mingw.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
