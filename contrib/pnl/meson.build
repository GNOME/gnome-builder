libpnl_headers = [
  'pnl-animation.h',
  'pnl-dock-bin.h',
  'pnl-dock-bin-edge.h',
  'pnl-dock-item.h',
  'pnl-dock-manager.h',
  'pnl-dock-overlay.h',
  'pnl-dock-overlay-edge.h',
  'pnl-dock-paned.h',
  'pnl-dock-revealer.h',
  'pnl-dock-stack.h',
  'pnl-dock-types.h',
  'pnl-dock-widget.h',
  'pnl-dock-window.h',
  'pnl-dock.h',
  'pnl-frame-source.h',
  'pnl-multi-paned.h',
  'pnl-tab-strip.h',
  'pnl-tab.h',
  'pnl.h',
]

libpnl_header_dir = get_option('includedir') + '/gnome-builder-' + meson.project_version() + '/pnl'

libpnl_conf = configuration_data()
libpnl_conf.set('MAJOR_VERSION', MAJOR_VERSION)
libpnl_conf.set('MINOR_VERSION', MINOR_VERSION)
libpnl_conf.set('MICRO_VERSION', MICRO_VERSION)
libpnl_conf.set('VERSION', meson.project_version())

libpnl_version_h = configure_file(
  input: 'pnl-version.h.in',
  output: 'pnl-version.h',
  configuration: libpnl_conf,
  install: true,
  install_dir: libpnl_header_dir,
)

install_headers(libpnl_headers,
  install_dir: libpnl_header_dir,
)

libpnl_resources = gnome.compile_resources('pnl-resources',
  'pnl.gresource.xml',
  c_name: 'pnl',
)

libpnl_sources = [
  'pnl-animation.c',
  'pnl-dock-bin-edge.c',
  'pnl-dock-bin-edge-private.h',
  'pnl-dock-bin.c',
  'pnl-dock-item.c',
  'pnl-dock-manager.c',
  'pnl-dock-overlay-edge.c',
  'pnl-dock-overlay.c',
  'pnl-dock-paned-private.h',
  'pnl-dock-paned.c',
  'pnl-dock-revealer.c',
  'pnl-dock-stack.c',
  'pnl-dock-tab-strip.c',
  'pnl-dock-tab-strip.h',
  'pnl-dock-transient-grab.c',
  'pnl-dock-transient-grab.h',
  'pnl-dock-widget.c',
  'pnl-dock-window.c',
  'pnl-dock.c',
  'pnl-frame-source.c',
  'pnl-multi-paned.c',
  'pnl-tab-strip.c',
  'pnl-tab.c',
  'pnl-util-private.h',
  'pnl-util.c',
  libpnl_version_h,
]

libpnl_deps = [
  dependency('gtk+-3.0'),
]

libpnl_link_args = '-Wl,--version-script=' + join_paths(meson.current_source_dir(), 'pnl.map')
libpnl = shared_library('panel-gtk', libpnl_sources + [libpnl_resources[0],libpnl_resources[1]],
  dependencies: libpnl_deps,
  c_args: ['-DPNL_COMPILATION'],
  link_args: libpnl_link_args,
  link_depends: 'pnl.map',
  version: '0.0.0',
  install: true,
  install_dir: pkglibdir,
)

libpnl_dep = declare_dependency(
  sources: libpnl_version_h,
  link_with: libpnl,
  include_directories: include_directories('.'),
)

if get_option('with_introspection')

  libpnl_gir = gnome.generate_gir(libpnl,
    sources: libpnl_sources + libpnl_headers,
    nsversion: '1.0',
    namespace: 'Pnl',
    symbol_prefix: 'pnl',
    identifier_prefix: 'Pnl',
    includes: ['Gdk-3.0', 'Gio-2.0', 'Gtk-3.0'],
    install: true,
    install_dir_gir: pkggirdir,
    install_dir_typelib: pkgtypelibdir,
    extra_args: [
      '--c-include=pnl.h',
      '-DPNL_COMPILATION',
    ],
  )

  if get_option('with_vapi')

    libpnl_vapi = gnome.generate_vapi('panel-gtk',
      sources: libpnl_gir[0],
      packages: ['gio-2.0', 'gtk+-3.0'],
      install: true,
      install_dir: pkgvapidir,
    )

  endif
endif
