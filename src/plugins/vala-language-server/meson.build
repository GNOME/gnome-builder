if get_option('plugin_vala_language_server')

plugin_vala_language_server_resources = gnome.compile_resources(
  'vala-language-server-resources',
  'vala-language-server.gresource.xml',
  c_name: 'gbp_vls',
)

plugins_sources += plugin_vala_language_server_resources

endif
