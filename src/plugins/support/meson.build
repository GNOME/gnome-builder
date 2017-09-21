if get_option('with_support')

support_resources = gnome.compile_resources(
  'ide-support-resources',
  'ide-support-resources.gresource.xml',
  c_name: 'ide_support',
)

support_sources = [
  'ide-support-application-addin.c',
  'ide-support-application-addin.h',
  'ide-support.c',
  'ide-support.h',
  'ide-support-plugin.c',
  support_resources[0],
]

shared_module('support-plugin', support_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

configure_file(
          input: 'support.plugin',
         output: 'support.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
