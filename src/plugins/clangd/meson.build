if get_option('plugin_clangd')

plugins_sources += files([
  'clangd-plugin.c',
  'gbp-clangd-code-action-provider.c',
  'gbp-clangd-completion-provider.c',
  'gbp-clangd-diagnostic-provider.c',
  'gbp-clangd-formatter.c',
  'gbp-clangd-highlighter.c',
  'gbp-clangd-hover-provider.c',
  'gbp-clangd-rename-provider.c',
  'gbp-clangd-symbol-resolver.c',
  'gbp-clangd-service.c',
])

plugin_clangd_resources = gnome.compile_resources(
  'clangd-resources',
  'clangd.gresource.xml',
  c_name: 'gbp_clangd',
)

plugins_sources += plugin_clangd_resources

endif
