if get_option('with_flatpak')

flatpak_res = gnome.compile_resources(
  'gbp-flatpak-resources',
  'gbp-flatpak-resources.gresource.xml',
  c_name: 'gbp_flatpak'
)

flatpak_sources = [
  flatpak_res,
  'gbp-flatpak-application-addin.c',
  'gbp-flatpak-application-addin.h',
  'gbp-flatpak-build-system-discovery.c',
  'gbp-flatpak-build-system-discovery.h',
  'gbp-flatpak-clone-widget.c',
  'gbp-flatpak-clone-widget.h',
  'gbp-flatpak-configuration.c',
  'gbp-flatpak-configuration.h',
  'gbp-flatpak-configuration-provider.c',
  'gbp-flatpak-configuration-provider.h',
  'gbp-flatpak-genesis-addin.c',
  'gbp-flatpak-genesis-addin.h',
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



flatpak_deps = plugin_deps + [
  dependency('flatpak', version: '>= 0.8.0'),
  dependency('libsoup-2.4', version: '>= 2.52.0'),
  libgit_dep,
]

# FIXME: Check for libgit thread and ssh support

shared_module('flatpak-plugin', flatpak_sources,
  dependencies: flatpak_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

install_data('flatpak.plugin', install_dir: plugindir)

endif
