if get_option('plugin_blueprint')

plugin_blueprint_resources = gnome.compile_resources(
  'blueprint-resources',
  'blueprint.gresource.xml',
  c_name: 'gbp_blueprint',
)

plugins_sources += plugin_blueprint_resources

endif
