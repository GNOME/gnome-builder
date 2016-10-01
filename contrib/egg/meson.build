libegg_headers = [
  'egg-animation.h',
  'egg-binding-group.h',
  'egg-box.h',
  'egg-centering-bin.h',
  'egg-column-layout.h',
  'egg-counter.h',
  'egg-date-time.h',
  'egg-empty-state.h',
  'egg-entry-box.h',
  'egg-file-chooser-entry.h',
  'egg-frame-source.h',
  'egg-heap.h',
  'egg-list-box.h',
  'egg-menu-manager.h',
  'egg-pill-box.h',
  'egg-priority-box.h',
  'egg-private.h',
  'egg-radio-box.h',
  'egg-scrolled-window.h',
  'egg-search-bar.h',
  'egg-settings-flag-action.h',
  'egg-settings-sandwich.h',
  'egg-signal-group.h',
  'egg-simple-label.h',
  'egg-simple-popover.h',
  'egg-slider.h',
  'egg-state-machine-buildable.h',
  'egg-state-machine.h',
  'egg-task-cache.h',
  'egg-three-grid.h',
  'egg-widget-action-group.h',
]

install_headers(libegg_headers,
  subdir: 'gnome-builder-' + meson.project_version() + '/egg'
)

libegg_resources = gnome.compile_resources('egg-resources', 'egg.gresource.xml',
  c_name: 'egg',
)

libegg_sources = [
  'egg-animation.c',
  'egg-binding-group.c',
  'egg-box.c',
  'egg-centering-bin.c',
  'egg-column-layout.c',
  'egg-counter.c',
  'egg-date-time.c',
  'egg-empty-state.c',
  'egg-entry-box.c',
  'egg-file-chooser-entry.c',
  'egg-frame-source.c',
  'egg-heap.c',
  'egg-list-box.c',
  'egg-menu-manager.c',
  'egg-pill-box.c',
  'egg-priority-box.c',
  'egg-radio-box.c',
  'egg-scrolled-window.c',
  'egg-search-bar.c',
  'egg-settings-flag-action.c',
  'egg-settings-sandwich.c',
  'egg-signal-group.c',
  'egg-simple-label.c',
  'egg-simple-popover.c',
  'egg-slider.c',
  'egg-state-machine-buildable.c',
  'egg-state-machine.c',
  'egg-task-cache.c',
  'egg-three-grid.c',
  'egg-widget-action-group.c',
]

libegg_deps = [
  libgio_dep,
  libgtk_dep,
  libm_dep,
  dependency('gmodule-2.0'),
]

libegg_args = []
if get_option('enable_rdtscp')
  libegg_args += '-DEGG_HAVE_RDTSCP'
endif

shm_dep = cc.find_library('rt', required: false)

libegg = shared_library('egg-private', libegg_sources + [libegg_resources[0], libegg_resources[1]],
  dependencies: libegg_deps + [shm_dep],
  c_args: libegg_args,
  version: '0.0.0',
  install: true,
  install_dir: pkglibdir,
)

libegg_dep = declare_dependency(
  link_with: libegg,
  dependencies: libegg_deps,
  include_directories: include_directories('.'),
)

if get_option('with_introspection')

  libegg_gir = gnome.generate_gir(libegg,
    sources: libegg_sources + libegg_headers,
    nsversion: '1.0',
    namespace: 'Egg',
    symbol_prefix: 'egg',
    identifier_prefix: 'Egg',
    includes: ['Gdk-3.0', 'Gio-2.0', 'Gtk-3.0'],
    install: true,
    install_dir_gir: pkggirdir,
    install_dir_typelib: pkgtypelibdir,
    extra_args: [
      '--c-include=egg-private.h',
    ]
  )

  if get_option('with_vapi')

    libegg_vapi = gnome.generate_vapi('egg-private',
      sources: libegg_gir[0],
      packages: ['gio-2.0', 'gtk+-3.0'],
      install: true,
      install_dir: pkgvapidir,
    )

  endif
endif
