if get_option('plugin_bash_language_server')

plugins_sources += files([
  'bash-language-server-plugin.c',
  'gbp-bash-code-action-provider.c',
  'gbp-bash-completion-provider.c',
  'gbp-bash-diagnostic-provider.c',
  'gbp-bash-formatter.c',
  'gbp-bash-highlighter.c',
  'gbp-bash-hover-provider.c',
  'gbp-bash-rename-provider.c',
  'gbp-bash-symbol-resolver.c',
  'gbp-bash-service.c',
])

plugin_bash_resources = gnome.compile_resources(
  'bash-language-server-resources',
  'bash-language-server.gresource.xml',
  c_name: 'gbp_bash',
)

plugins_sources += plugin_bash_resources

endif
