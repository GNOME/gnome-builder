plugins_sources += files([
  'line-spacing-plugin.c',
  'gbp-line-spacing-buffer-addin.c',
])

plugin_line_spacing_resources = gnome.compile_resources(
  'gbp-line-spacing-resources',
  'line-spacing.gresource.xml',
  c_name: 'gbp_line_spacing',
)

plugins_sources += plugin_line_spacing_resources
