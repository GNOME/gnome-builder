if get_option('plugin_buildstream')

plugins_sources += files([
  'buildstream-plugin.c',
  'gbp-buildstream-build-system-discovery.c',
  'gbp-buildstream-build-system.c',
  'gbp-buildstream-pipeline-addin.c',
])

plugin_buildstream_resources = gnome.compile_resources(
  'buildstream-resources',
  'buildstream.gresource.xml',
  c_name: 'gbp_buildstream',
)

plugins_sources += plugin_buildstream_resources

endif
