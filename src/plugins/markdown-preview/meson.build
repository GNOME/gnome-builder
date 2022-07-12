if get_option('plugin_markdown_preview')

if not get_option('webkit').enabled()
  error('-Dwebkit=enabled is required for markdown-preview plugin')
endif

plugins_sources += files([
  'markdown-preview-plugin.c',
  'gbp-markdown-html-generator.c',
  'gbp-markdown-preview-workspace-addin.c',
])

plugin_markdown_preview_resources = gnome.compile_resources(
  'markdown-preview-resources',
  'markdown-preview.gresource.xml',
  c_name: 'gbp_markdown_preview'
)

plugins_sources += plugin_markdown_preview_resources

endif
