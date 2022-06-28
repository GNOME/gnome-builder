if get_option('plugin_podman')

plugins_sources += files([
  'podman-plugin.c',
  'gbp-podman-runtime.c',
  'gbp-podman-runtime-provider.c',
])

plugin_podman_resources = gnome.compile_resources(
  'podman-resources',
  'podman.gresource.xml',
  c_name: 'gbp_podman'
)

plugins_sources += plugin_podman_resources

test_sources_podman = [
  'test-storage-configuration.c',
  'gbp-podman-runtime.c',
]

test_env_podman = [
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
]

test_podman = executable('test-podman', test_sources_podman,
        c_args: test_cflags,
  dependencies: [ libide_io_dep, libide_foundry_dep, libjson_glib_dep ],
)
test('test-podman', test_podman, env: test_env_podman)

endif
