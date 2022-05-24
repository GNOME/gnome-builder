if get_option('plugin_jdtls')

plugins_sources += files([
  'jdtls-plugin.c',
  'gbp-jdtls-code-action-provider.c',
  'gbp-jdtls-completion-provider.c',
  'gbp-jdtls-diagnostic-provider.c',
  'gbp-jdtls-formatter.c',
  'gbp-jdtls-highlighter.c',
  'gbp-jdtls-hover-provider.c',
  'gbp-jdtls-rename-provider.c',
  'gbp-jdtls-symbol-resolver.c',
  'gbp-jdtls-service.c',
])

plugin_jdtls_resources = gnome.compile_resources(
  'jdtls-resources',
  'jdtls.gresource.xml',
  c_name: 'gbp_jdtls',
)

plugins_sources += plugin_jdtls_resources

endif
