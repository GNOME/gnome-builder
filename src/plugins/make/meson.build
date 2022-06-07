if get_option('plugin_make')

plugins_sources += files([
  'make-plugin.c',
  'gbp-make-build-system-discovery.c',
  'gbp-make-build-system.c',
  'gbp-make-build-target.c',
  'gbp-make-build-target-provider.c',
  'gbp-make-pipeline-addin.c',
  'gbp-make-run-command-provider.c',
])

plugin_make_resources = gnome.compile_resources(
  'make-resources',
  'make.gresource.xml',
  c_name: 'gbp_make',
)

plugins_sources += plugin_make_resources

endif
