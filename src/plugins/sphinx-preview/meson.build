if get_option('plugin_sphinx_preview')

if not get_option('webkit').enabled()
  error('-Dwebkit=enabled is required for sphinx-preview plugin')
endif

plugins_sources += files([
  'sphinx-preview-plugin.c',
  'gbp-rst-html-generator.c',
  'gbp-sphinx-compiler.c',
  'gbp-sphinx-html-generator.c',
  'gbp-sphinx-preview-workspace-addin.c',
])

plugin_sphinx_preview_resources = gnome.compile_resources(
  'sphinx-preview-resources',
  'sphinx-preview.gresource.xml',
  c_name: 'gbp_sphinx_preview'
)

plugins_sources += plugin_sphinx_preview_resources

endif
