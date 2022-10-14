if get_option('plugin_serve_d')

plugin_served_resources = gnome.compile_resources(
  'serve-d-resources',
  'serve-d.gresource.xml',
  c_name: 'gbp_served',
)

plugins_sources += plugin_served_resources

endif
