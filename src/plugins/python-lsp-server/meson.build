if get_option('plugin_python_lsp_server')

plugin_pylsp_resources = gnome.compile_resources(
  'pylsp-language-server-resources',
  'pylsp-language-server.gresource.xml',
  c_name: 'gbp_pylsp',
)

plugins_sources += plugin_pylsp_resources

endif
