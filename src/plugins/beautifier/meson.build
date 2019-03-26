if get_option('plugin_beautifier')

plugins_sources += files([
  'beautifier-plugin.c',
  'gb-beautifier-config.c',
  'gb-beautifier-helper.c',
  'gb-beautifier-process.c',
  'gb-beautifier-editor-addin.c',
])

plugin_beautifier_resources = gnome.compile_resources(
  'beautifier-resources',
  'beautifier.gresource.xml',
  c_name: 'gbp_beautifier',
)

plugins_sources += plugin_beautifier_resources

endif
