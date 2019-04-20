plugins_sources += files([
  'omni-gutter-plugin.c',
  'gbp-omni-gutter-renderer.c',
  'gbp-omni-gutter-editor-page-addin.c',
])

omni_gutter_resources = gnome.compile_resources(
  'omni-gutter-resources',
  'omni-gutter.gresource.xml',
  c_name: 'gbp_omni_gutter',
)

plugins_sources += omni_gutter_resources
