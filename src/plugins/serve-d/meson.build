if get_option('plugin_serve_d')

plugins_sources += files([
  'serve-d-plugin.c',
  'gbp-served-code-action-provider.c',
  'gbp-served-completion-provider.c',
  'gbp-served-diagnostic-provider.c',
  'gbp-served-formatter.c',
  'gbp-served-highlighter.c',
  'gbp-served-hover-provider.c',
  'gbp-served-rename-provider.c',
  'gbp-served-symbol-resolver.c',
  'gbp-served-service.c',
])

plugin_served_resources = gnome.compile_resources(
  'serve-d-resources',
  'serve-d.gresource.xml',
  c_name: 'gbp_served',
)

plugins_sources += plugin_served_resources

endif
