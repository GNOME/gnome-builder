if get_option('with_support')

support_resources = gnome.compile_resources(
  'support-resources',
  'support.gresource.xml',
  c_name: 'ide_support',
)

support_sources = [
  'ide-support-application-addin.c',
  'ide-support-application-addin.h',
  'ide-support.c',
  'ide-support.h',
  'ide-support-plugin.c',
]

gnome_builder_plugins_sources += files(support_sources)
gnome_builder_plugins_sources += support_resources[0]

endif
