if get_option('plugin_meson')

plugins_sources += files([
  'gbp-meson-build-stage-cross-file.c',
  'gbp-meson-build-system-discovery.c',
  'gbp-meson-build-system.c',
  'gbp-meson-build-target-provider.c',
  'gbp-meson-build-target.c',
  'gbp-meson-introspection.c',
  'gbp-meson-pipeline-addin.c',
  'gbp-meson-run-command-provider.c',
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

test_meson_sources = files([
  'test-meson-build-system.c',
  'gbp-meson-build-system.c',
  'gbp-meson-toolchain.c',
  'gbp-meson-utils.c',
])

config_h.set10('PLUGIN_MESON', true)

test_meson = executable('test-meson', test_meson_sources,
        c_args: test_cflags,
  dependencies: [ libide_foundry_dep, libide_terminal_dep ],
)
test('test-meson', test_meson, env: test_env)

endif
