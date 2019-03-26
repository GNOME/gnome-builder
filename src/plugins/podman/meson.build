if get_option('plugin_podman')

plugins_sources += files([
  'podman-plugin.c',
  'gbp-podman-runtime.c',
  'gbp-podman-runtime-provider.c',
  'gbp-podman-subprocess-launcher.c',
])

plugin_podman_resources = gnome.compile_resources(
  'podman-resources',
  'podman.gresource.xml',
  c_name: 'gbp_podman'
)

plugins_sources += plugin_podman_resources

endif
