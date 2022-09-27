if get_option('plugin_python_lsp_server')

plugins_sources += files([
  'pylsp-language-server-plugin.c',
  'gbp-pylsp-code-action-provider.c',
  'gbp-pylsp-completion-provider.c',
  'gbp-pylsp-diagnostic-provider.c',
  'gbp-pylsp-formatter.c',
  'gbp-pylsp-highlighter.c',
  'gbp-pylsp-hover-provider.c',
  'gbp-pylsp-rename-provider.c',
  'gbp-pylsp-symbol-resolver.c',
  'gbp-pylsp-service.c',
])

plugin_pylsp_resources = gnome.compile_resources(
  'pylsp-language-server-resources',
  'pylsp-language-server.gresource.xml',
  c_name: 'gbp_pylsp',
)

plugins_sources += plugin_pylsp_resources

endif
