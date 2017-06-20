if get_option('with_gcc')

gcc_sources = [
  'gbp-gcc-pipeline-addin.c',
  'gbp-gcc-pipeline-addin.h',
  'gbp-gcc-plugin.c',
]

shared_module('gcc-plugin', gcc_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

configure_file(
          input: 'gcc.plugin',
         output: 'gcc.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
