if get_option('plugin_gopls')

plugins_sources += files([
  'gopls-plugin.c',
  'gbp-gopls-code-action-provider.c',
  'gbp-gopls-completion-provider.c',
  'gbp-gopls-diagnostic-provider.c',
  'gbp-gopls-formatter.c',
  'gbp-gopls-highlighter.c',
  'gbp-gopls-hover-provider.c',
  'gbp-gopls-rename-provider.c',
  'gbp-gopls-symbol-resolver.c',
  'gbp-gopls-service.c',
])

plugin_gopls_resources = gnome.compile_resources(
  'gopls-resources',
  'gopls.gresource.xml',
  c_name: 'gbp_gopls',
)

plugins_sources += plugin_gopls_resources

endif
