if get_option('plugin_flatpak')

if not get_option('plugin_git')
  error('-Dplugin_git=true is required for flatpak')
endif

if not libportal_dep.found()
  error('plugin_flatpak requires access to libportal')
endif

libostree_dep = dependency('ostree-1')
libsoup_dep = dependency('libsoup-2.4', version: '>= 2.52.0')

subdir('daemon')

plugins_sources += files([
  'flatpak-plugin.c',
  'gbp-flatpak-aux.c',
  'gbp-flatpak-build-system-discovery.c',
  'gbp-flatpak-build-target-provider.c',
  'gbp-flatpak-build-target.c',
  'gbp-flatpak-client.c',
  'gbp-flatpak-clone-widget.c',
  'gbp-flatpak-config-provider.c',
  'gbp-flatpak-dependency-updater.c',
  'gbp-flatpak-download-stage.c',
  'gbp-flatpak-install-dialog.c',
  'gbp-flatpak-manifest.c',
  'gbp-flatpak-pipeline-addin.c',
  'gbp-flatpak-runner.c',
  'gbp-flatpak-runtime-provider.c',
  'gbp-flatpak-runtime.c',
  'gbp-flatpak-sdk-stage.c',
  'gbp-flatpak-sources.c',
  'gbp-flatpak-subprocess-launcher.c',
  'gbp-flatpak-util.c',
  'ipc-flatpak-transfer-impl.c',
])

plugin_flatpak_resources = gnome.compile_resources(
  'flatpak-resources',
  'flatpak.gresource.xml',
  c_name: 'gbp_flatpak'
)

plugins_deps += [
  libostree_dep,
  libportal_dep,
  libsoup_dep,
]

plugins_sources += plugin_flatpak_resources

endif
