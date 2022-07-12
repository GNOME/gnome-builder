if get_option('plugin_blueprint')

plugins_sources += files([
  'blueprint-plugin.c',
  'gbp-blueprint-code-action-provider.c',
  'gbp-blueprint-completion-provider.c',
  'gbp-blueprint-diagnostic-provider.c',
  'gbp-blueprint-hover-provider.c',
  'gbp-blueprint-service.c',
])

plugin_blueprint_resources = gnome.compile_resources(
  'blueprint-resources',
  'blueprint.gresource.xml',
  c_name: 'gbp_blueprint',
)

plugins_sources += plugin_blueprint_resources

endif
