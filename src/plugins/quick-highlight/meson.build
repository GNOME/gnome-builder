if get_option('plugin_quick_highlight')

plugins_sources += files([
  'quick-highlight-plugin.c',
  'gbp-quick-highlight-editor-page-addin.c',
])

plugin_quick_highlight_resources = gnome.compile_resources(
  'quick-highlight-resources',
  'quick-highlight.gresource.xml',
  c_name: 'gbp_quick_highlight',
)

plugins_sources += plugin_quick_highlight_resources

endif
