plugins_sources += files([
  'host-plugin.c',
  'gbp-host-runtime.c',
  'gbp-host-runtime-provider.c',
  'gbp-noop-runtime.c',
])

plugin_host_resources = gnome.compile_resources(
  'host-resources',
  'host.gresource.xml',
  c_name: 'gbp_host',
)

plugins_sources += plugin_host_resources
