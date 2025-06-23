plugins_sources += files([
  'css-preview-plugin.c',
  'gbp-css-preview-hover-provider.c',
])

plugin_css_preview_resources = gnome.compile_resources(
  'gbp-css-preview-resources',
  'css-preview.gresource.xml',
  c_name: 'css_preview',
)

plugins_sources += plugin_css_preview_resources
