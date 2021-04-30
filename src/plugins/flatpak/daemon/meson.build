gnome_builder_flatpak_deps = [
  libgiounix_dep,
  libflatpak_dep,
  libostree_dep,
  libsoup_dep,
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

gnome_builder_flatpak_sources = [
  'gnome-builder-flatpak.c',
  'ipc-flatpak-service-impl.c',
  ipc_flatpak_service_src,
  ipc_flatpak_transfer_src,
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

test_flatpak = executable('test-flatpak', 'test-flatpak.c', test_flatpak_sources,
  dependencies: [ libgiounix_dep ],
)
