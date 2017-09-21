if get_option('with_gnome_code_assistance')

gnome_code_assistance_sources = [
  'gca-diagnostics.c',
  'gca-diagnostics.h',
  'gca-service.c',
  'gca-service.h',
  'gca-structs.c',
  'gca-structs.h',
  'gca-plugin.c',
  'ide-gca-diagnostic-provider.c',
  'ide-gca-diagnostic-provider.h',
  'ide-gca-preferences-addin.c',
  'ide-gca-preferences-addin.h',
  'ide-gca-service.c',
  'ide-gca-service.h',
]

shared_module('gnome-code-assistance-plugin', gnome_code_assistance_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

configure_file(
          input: 'gnome-code-assistance.plugin',
         output: 'gnome-code-assistance.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

install_data('org.gnome.builder.gnome-code-assistance.gschema.xml',
  install_dir: schema_dir)

endif
