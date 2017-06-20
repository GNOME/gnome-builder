if get_option('with_sysmon')

sysmon_resources = gnome.compile_resources(
  'gb-sysmon-resources',
  'gb-sysmon.gresource.xml',
  c_name: 'gb_sysmon',
)

sysmon_sources = [
  sysmon_resources[0],
  sysmon_resources[1],
  'gb-sysmon-panel.c',
  'gb-sysmon-panel.h',
  'gb-sysmon-addin.c',
  'gb-sysmon-addin.h',
]

shared_module('sysmon', sysmon_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

configure_file(
          input: 'sysmon.plugin',
         output: 'sysmon.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
