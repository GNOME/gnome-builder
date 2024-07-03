if get_option('plugin_folding')

plugins_sources += files([
  'folding-plugin.c',
  'folding-buffer-addin.c',
  'folding-gutter-renderer.c',
  'folding-editor-page-addin.c',
])

plugin_folding_resources = gnome.compile_resources(
  'folding-resources',
  'folding.gresource.xml',
  c_name: 'folding',
)

plugins_sources += plugin_folding_resources

endif
