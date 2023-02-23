if get_option('plugin_golang')

plugins_sources += files([
  'golang-plugin.c',
  'gbp-golang-build-system-discovery.c',
  'gbp-golang-build-system.c',
  'gbp-golang-dependency-updater.c',
  'gbp-golang-pipeline-addin.c',
  'gbp-golang-run-command-provider.c',
])

plugin_golang_resources = gnome.compile_resources(
  'golang-resources',
  'golang.gresource.xml',
  c_name: 'gbp_golang',
)

plugins_sources += plugin_golang_resources

endif
