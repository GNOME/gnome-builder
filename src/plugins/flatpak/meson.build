if get_option('plugin_flatpak')

if not get_option('plugin_git')
  error('-Dplugin_git=true is required for flatpak')
endif

plugins_sources += files([
  'flatpak-plugin.c',
  'gbp-flatpak-application-addin.c',
  'gbp-flatpak-build-system-discovery.c',
  'gbp-flatpak-build-target-provider.c',
  'gbp-flatpak-build-target.c',
  'gbp-flatpak-clone-widget.c',
  'gbp-flatpak-config-provider.c',
  'gbp-flatpak-dependency-updater.c',
  'gbp-flatpak-download-stage.c',
  'gbp-flatpak-install-dialog.c',
  'gbp-flatpak-manifest.c',
  'gbp-flatpak-pipeline-addin.c',
  'gbp-flatpak-preferences-addin.c',
  'gbp-flatpak-runner.c',
  'gbp-flatpak-runtime-provider.c',
  'gbp-flatpak-runtime.c',
  'gbp-flatpak-sources.c',
  'gbp-flatpak-subprocess-launcher.c',
  'gbp-flatpak-transfer.c',
  'gbp-flatpak-util.c',
  'gbp-flatpak-workbench-addin.c',
])

plugin_flatpak_resources = gnome.compile_resources(
  'flatpak-resources',
  'flatpak.gresource.xml',
  c_name: 'gbp_flatpak'
)

plugins_deps += [
  dependency('flatpak', version: '>= 0.8.0'),
  dependency('ostree-1'),
  dependency('libsoup-2.4', version: '>= 2.52.0'),
]

plugins_sources += plugin_flatpak_resources

endif
