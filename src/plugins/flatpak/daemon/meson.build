libflatpak_dep = dependency('flatpak', version: '>= 1.10.2')

gnome_builder_flatpak_deps = [
  libgiounix_dep,
  libflatpak_dep,
  libostree_dep,
]

ipc_flatpak_service_src = gnome.gdbus_codegen('ipc-flatpak-service',
           sources: 'org.gnome.Builder.Flatpak.Service.xml',
  interface_prefix: 'org.gnome.Builder.',
         namespace: 'Ipc',
)

ipc_flatpak_transfer_src = gnome.gdbus_codegen('ipc-flatpak-transfer',
           sources: 'org.gnome.Builder.Flatpak.Transfer.xml',
  interface_prefix: 'org.gnome.Builder.',
         namespace: 'Ipc',
)

ipc_flatpak_resources = gnome.compile_resources(
  'ipc-resources',
  'ipc-flatpak.gresource.xml',
  c_name: 'ipc_flatpak'
)

gnome_builder_flatpak_sources = [
  'gnome-builder-flatpak.c',
  'ipc-flatpak-service-impl.c',
  'ipc-flatpak-repo.c',
  ipc_flatpak_service_src,
  ipc_flatpak_transfer_src,
  ipc_flatpak_resources,
]

gnome_builder_flatpak = executable('gnome-builder-flatpak', gnome_builder_flatpak_sources,
           install: true,
       install_dir: get_option('libexecdir'),
      dependencies: gnome_builder_flatpak_deps,
)

test_flatpak_sources = [
  'test-flatpak.c',
  ipc_flatpak_service_src,
  ipc_flatpak_transfer_src,
]

test_install_sources = [
  'test-install.c',
  ipc_flatpak_service_src,
  ipc_flatpak_transfer_src,
]

plugins_sources += [
  ipc_flatpak_service_src,
  ipc_flatpak_transfer_src,
]

test_flatpak = executable('test-flatpak', 'test-flatpak.c', test_flatpak_sources,
  dependencies: [ libgiounix_dep ],
)

test_install = executable('test-install', 'test-install.c', test_install_sources,
  dependencies: [ libgiounix_dep ],
)

# test('Test flatpak daemon', test_flatpak, depends: gnome_builder_flatpak, workdir: meson.current_build_dir())
# test('Test flatpak daemon install', test_install, depends: gnome_builder_flatpak, workdir: meson.current_build_dir(), args: 'org.freedesktop.Sdk.Extension.rust-stable//20.08')
