if get_option('with_flatpak')

flatpak_resources = gnome.compile_resources(
  'flatpak-resources',
  'flatpak.gresource.xml',
  c_name: 'gbp_flatpak'
)

flatpak_sources = [
  'gbp-flatpak-application-addin.c',
  'gbp-flatpak-application-addin.h',
  'gbp-flatpak-build-system-discovery.c',
  'gbp-flatpak-build-system-discovery.h',
  'gbp-flatpak-build-target.c',
  'gbp-flatpak-build-target.h',
  'gbp-flatpak-build-target-provider.c',
  'gbp-flatpak-build-target-provider.h',
  'gbp-flatpak-clone-widget.c',
  'gbp-flatpak-clone-widget.h',
  'gbp-flatpak-configuration-provider.c',
  'gbp-flatpak-configuration-provider.h',
  'gbp-flatpak-dependency-updater.c',
  'gbp-flatpak-dependency-updater.h',
  'gbp-flatpak-download-stage.c',
  'gbp-flatpak-download-stage.h',
  'gbp-flatpak-genesis-addin.c',
  'gbp-flatpak-genesis-addin.h',
  'gbp-flatpak-manifest.c',
  'gbp-flatpak-manifest.h',
  'gbp-flatpak-pipeline-addin.c',
  'gbp-flatpak-pipeline-addin.h',
  'gbp-flatpak-preferences-addin.c',
  'gbp-flatpak-preferences-addin.h',
  'gbp-flatpak-plugin.c',
  'gbp-flatpak-runner.c',
  'gbp-flatpak-runner.h',
  'gbp-flatpak-runtime-provider.c',
  'gbp-flatpak-runtime-provider.h',
  'gbp-flatpak-runtime.c',
  'gbp-flatpak-runtime.h',
  'gbp-flatpak-sources.c',
  'gbp-flatpak-sources.h',
  'gbp-flatpak-subprocess-launcher.c',
  'gbp-flatpak-subprocess-launcher.h',
  'gbp-flatpak-transfer.c',
  'gbp-flatpak-transfer.h',
  'gbp-flatpak-util.c',
  'gbp-flatpak-util.h',
  'gbp-flatpak-workbench-addin.c',
  'gbp-flatpak-workbench-addin.h',
]

gnome_builder_plugins_deps += [
  dependency('flatpak', version: '>= 0.8.0'),
  dependency('libsoup-2.4', version: '>= 2.52.0'),
  libgit_dep,
]

gnome_builder_plugins_sources += files(flatpak_sources)
gnome_builder_plugins_sources += flatpak_resources[0]

endif
