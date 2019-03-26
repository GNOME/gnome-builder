if get_option('plugin_gettext')

plugins_sources += files([
  'gettext-plugin.c',
  'ide-gettext-diagnostic-provider.c',
])

plugin_gettext_resources = gnome.compile_resources(
  'gettext-resources',
  'gettext.gresource.xml',
  c_name: 'gbp_gettext',
)

plugins_sources += plugin_gettext_resources

endif
