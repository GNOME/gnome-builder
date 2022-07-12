if get_option('plugin_html_preview')

if not get_option('webkit').enabled()
  error('-Dwebkit=enabled is required for html-preview plugin')
endif

plugins_sources += files([
  'html-preview-plugin.c',
  'gbp-html-preview-workspace-addin.c',
])

plugin_html_preview_resources = gnome.compile_resources(
  'html-preview-resources',
  'html-preview.gresource.xml',
  c_name: 'gbp_html_preview'
)

plugins_sources += plugin_html_preview_resources

endif
