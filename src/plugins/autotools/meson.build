if get_option('plugin_autotools')

plugins_sources += files([
  'autotools-plugin.c',
  'ide-autotools-autogen-stage.c',
  'ide-autotools-build-system.c',
  'gbp-autotools-build-system-discovery.c',
  'ide-autotools-build-target-provider.c',
  'ide-autotools-build-target.c',
  'ide-autotools-make-stage.c',
  'ide-autotools-makecache-stage.c',
  'ide-autotools-pipeline-addin.c',
  'ide-makecache-target.c',
  'ide-makecache.c',
])

plugin_autotools_resources = gnome.compile_resources(
  'gbp-autotools-resources',
  'autotools.gresource.xml',
  c_name: 'gbp_autotools',
)

plugins_sources += plugin_autotools_resources

endif
