if get_option('plugin_python_pack')

plugins_sources += files([
  'ide-python-indenter.c',
  'python-pack-plugin.c',
])

plugin_python_pack_resources = gnome.compile_resources(
  'gbp-python-pack-resources',
  'python-pack.gresource.xml',
  c_name: 'gbp_python_pack',
)

plugins_sources += plugin_python_pack_resources

endif
