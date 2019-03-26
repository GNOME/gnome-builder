if get_option('plugin_meson')

plugins_sources += files([
  'gbp-meson-build-stage-cross-file.c',
  'gbp-meson-build-system-discovery.c',
  'gbp-meson-build-system.c',
  'gbp-meson-build-target-provider.c',
  'gbp-meson-build-target.c',
  'gbp-meson-config-view-addin.c',
  'gbp-meson-pipeline-addin.c',
  'gbp-meson-test-provider.c',
  'gbp-meson-test.c',
  'gbp-meson-tool-row.c',
  'gbp-meson-toolchain-edition-preferences-addin.c',
  'gbp-meson-toolchain-edition-preferences-row.c',
  'gbp-meson-toolchain-provider.c',
  'gbp-meson-toolchain.c',
  'gbp-meson-utils.c',
  'meson-plugin.c',
])

plugin_meson_resources = gnome.compile_resources(
  'gbp-meson-resources',
  'meson.gresource.xml',
  c_name: 'gbp_meson',
)

plugins_sources += plugin_meson_resources

endif
