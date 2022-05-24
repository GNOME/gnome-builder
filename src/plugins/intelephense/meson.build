if get_option('plugin_intelephense')

plugins_sources += files([
  'intelephense-plugin.c',
  'gbp-intelephense-code-action-provider.c',
  'gbp-intelephense-completion-provider.c',
  'gbp-intelephense-diagnostic-provider.c',
  'gbp-intelephense-formatter.c',
  'gbp-intelephense-highlighter.c',
  'gbp-intelephense-hover-provider.c',
  'gbp-intelephense-rename-provider.c',
  'gbp-intelephense-symbol-resolver.c',
  'gbp-intelephense-service.c',
])

plugin_intelephense_resources = gnome.compile_resources(
  'intelephense-resources',
  'intelephense.gresource.xml',
  c_name: 'gbp_intelephense',
)

plugins_sources += plugin_intelephense_resources

endif
