if get_option('plugin_markdown_indenter')

plugins_sources += files([
  'gbp-markdown-indenter.c',
  'markdown-indenter-plugin.c',
])

plugin_markdown_indenter_resources = gnome.compile_resources(
  'markdown-indenter-resources',
  'markdown-indenter.gresource.xml',
  c_name: 'gbp_markdown_indenter',
)

plugins_sources += plugin_markdown_indenter_resources

endif
