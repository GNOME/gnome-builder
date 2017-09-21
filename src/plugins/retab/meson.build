if get_option('with_retab')

retab_resources = gnome.compile_resources(
  'gbp-retab-resources',
  'gbp-retab.gresource.xml',
  c_name: 'gbp_retab',
)

retab_sources = [
  'gbp-retab-plugin.c',
  'gbp-retab-view-addin.c',
  'gbp-retab-view-addin.h',
  retab_resources[0],
]

shared_module('retab-plugin', retab_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

configure_file(
          input: 'retab.plugin',
         output: 'retab.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
