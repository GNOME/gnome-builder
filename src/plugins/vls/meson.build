if get_option('plugin_vls')

plugins_sources += files([
  'vls-plugin.c',
  'gbp-vls-code-action-provider.c',
  'gbp-vls-completion-provider.c',
  'gbp-vls-diagnostic-provider.c',
  'gbp-vls-formatter.c',
  'gbp-vls-highlighter.c',
  'gbp-vls-hover-provider.c',
  'gbp-vls-rename-provider.c',
  'gbp-vls-search-provider.c',
  'gbp-vls-symbol-resolver.c',
  'gbp-vls-service.c',
])

plugin_vls_resources = gnome.compile_resources(
  'vls-resources',
  'vls.gresource.xml',
  c_name: 'gbp_vls',
)

plugins_sources += plugin_vls_resources

endif
