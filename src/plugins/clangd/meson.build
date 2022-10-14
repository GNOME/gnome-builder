if get_option('plugin_clangd')

plugin_clangd_resources = gnome.compile_resources(
  'clangd-resources',
  'clangd.gresource.xml',
  c_name: 'gbp_clangd',
)

plugins_sources += plugin_clangd_resources

endif
