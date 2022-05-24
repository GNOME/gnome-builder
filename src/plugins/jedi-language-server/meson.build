if get_option('plugin_jedi_language_server')

plugins_sources += files([
  'jedi-language-server-plugin.c',
  'gbp-jedi-code-action-provider.c',
  'gbp-jedi-completion-provider.c',
  'gbp-jedi-diagnostic-provider.c',
  'gbp-jedi-formatter.c',
  'gbp-jedi-highlighter.c',
  'gbp-jedi-hover-provider.c',
  'gbp-jedi-rename-provider.c',
  'gbp-jedi-symbol-resolver.c',
  'gbp-jedi-service.c',
])

plugin_jedi_resources = gnome.compile_resources(
  'jedi-language-server-resources',
  'jedi-language-server.gresource.xml',
  c_name: 'gbp_jedi',
)

plugins_sources += plugin_jedi_resources

endif
