if get_option('plugin_glsl_language_server')

plugins_sources += files([
  'glsl-language-server-plugin.c',
  'gbp-glsl-diagnostic-provider.c',
  'gbp-glsl-service.c',
])

plugin_glsl_resources = gnome.compile_resources(
  'glsl-language-server-resources',
  'glsl-language-server.gresource.xml',
  c_name: 'gbp_glsl',
)

plugins_sources += plugin_glsl_resources

endif
