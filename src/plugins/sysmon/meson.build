if get_option('with_sysmon')

sysmon_resources = gnome.compile_resources(
  'gb-sysmon-resources',
  'sysmon.gresource.xml',
  c_name: 'gb_sysmon',
)

sysmon_sources = [
  'gb-sysmon-panel.c',
  'gb-sysmon-panel.h',
  'gb-sysmon-addin.c',
  'gb-sysmon-addin.h',
]

gnome_builder_plugins_sources += files(sysmon_sources)
gnome_builder_plugins_sources += sysmon_resources[0]

endif
