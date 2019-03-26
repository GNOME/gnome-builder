if get_option('plugin_retab')

plugins_sources += files([
  'retab-plugin.c',
  'gbp-retab-editor-page-addin.c',
])

plugin_retab_resources = gnome.compile_resources(
  'retab-resources',
  'retab.gresource.xml',
  c_name: 'gbp_retab',
)

plugins_sources += plugin_retab_resources

endif
