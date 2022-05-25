if get_option('plugin_cargo')

plugins_sources += files([
  'cargo-plugin.c',
  'gbp-cargo-build-system-discovery.c',
  'gbp-cargo-build-system.c',
  'gbp-cargo-dependency-updater.c',
  'gbp-cargo-pipeline-addin.c',
  'gbp-cargo-run-command-provider.c',
])

plugin_cargo_resources = gnome.compile_resources(
  'cargo-resources',
  'cargo.gresource.xml',
  c_name: 'gbp_cargo',
)

plugins_sources += plugin_cargo_resources

endif
