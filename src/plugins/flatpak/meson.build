if get_option('plugin_flatpak')

if not get_option('plugin_git')
  error('-Dplugin_git=true is required for flatpak')
endif

if not libportal_dep.found()
  error('plugin_flatpak requires access to libportal')
endif

libostree_dep = dependency('ostree-1')
libsoup_dep = dependency('libsoup-3.0')
libyaml_dep = dependency('yaml-0.1')

subdir('daemon')

plugins_sources += files([
  'flatpak-plugin.c',
  'gbp-flatpak-aux.c',
  'gbp-flatpak-build-system-discovery.c',
  'gbp-flatpak-client.c',
  'gbp-flatpak-config-provider.c',
  'gbp-flatpak-dependency-updater.c',
  'gbp-flatpak-download-stage.c',
  'gbp-flatpak-install-dialog.c',
  'gbp-flatpak-manifest.c',
  'gbp-flatpak-pipeline-addin.c',
  'gbp-flatpak-run-command-provider.c',
  'gbp-flatpak-runtime-provider.c',
  'gbp-flatpak-runtime.c',
  'gbp-flatpak-sdk.c',
  'gbp-flatpak-sdk-provider.c',
  'gbp-flatpak-sdk-stage.c',
  'gbp-flatpak-sources.c',
  'gbp-flatpak-tweaks-addin.c',
  'gbp-flatpak-util.c',
  'gbp-flatpak-workbench-addin.c',
  'ipc-flatpak-transfer-impl.c',
])

config_h.set10('HAVE_FLATPAK', true)

plugin_flatpak_resources = gnome.compile_resources(
  'flatpak-resources',
  'flatpak.gresource.xml',
  c_name: 'gbp_flatpak'
)

plugins_deps += [
  libostree_dep,
  libportal_dep,
  libsoup_dep,
  libyaml_dep,
]

plugins_sources += plugin_flatpak_resources

install_data(['org.gnome.builder.flatpak.gschema.xml'], install_dir: schema_dir)

endif
