if get_option('with_gettext')

gettext_resources = gnome.compile_resources(
  'gettext-resources',
  'gettext.gresource.xml',
  c_name: 'ide_gettext',
)

gettext_sources = [
  'ide-gettext-diagnostic-provider.c',
  'ide-gettext-diagnostic-provider.h',
  'gettext-plugin.c',
]

gnome_builder_plugins_sources += files(gettext_sources)
gnome_builder_plugins_sources += gettext_resources[0]

endif
