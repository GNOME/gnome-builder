if get_option('plugin_ts_language_server')

plugin_tslsp_resources = gnome.compile_resources(
  'ts-language-server-resources',
  'ts-language-server.gresource.xml',
  c_name: 'gbp_tslsp',
)

plugins_sources += plugin_tslsp_resources

endif
