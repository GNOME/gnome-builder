if get_option('plugin_ts_language_server')

plugins_sources += files([
  'ts-language-server-plugin.c',
  'gbp-tslsp-code-action-provider.c',
  'gbp-tslsp-completion-provider.c',
  'gbp-tslsp-diagnostic-provider.c',
  'gbp-tslsp-formatter.c',
  'gbp-tslsp-highlighter.c',
  'gbp-tslsp-hover-provider.c',
  'gbp-tslsp-rename-provider.c',
  'gbp-tslsp-symbol-resolver.c',
  'gbp-tslsp-service.c',
])

plugin_tslsp_resources = gnome.compile_resources(
  'ts-language-server-resources',
  'ts-language-server.gresource.xml',
  c_name: 'gbp_tslsp',
)

plugins_sources += plugin_tslsp_resources

endif
