librg_sources = [
  'realtime-graphs.h',
  'rg-column.c',
  'rg-column.h',
  'rg-column-private.h',
  'rg-cpu-graph.c',
  'rg-cpu-graph.h',
  'rg-cpu-table.c',
  'rg-cpu-table.h',
  'rg-graph.c',
  'rg-graph.h',
  'rg-line-renderer.c',
  'rg-line-renderer.h',
  'rg-renderer.c',
  'rg-renderer.h',
  'rg-ring.c',
  'rg-ring.h',
  'rg-table.c',
  'rg-table.h',
]

librg_deps = [
  libgio_dep,
  libgtk_dep,
  libdazzle_dep,
]

librg_link_args = '-Wl,--version-script=' + join_paths(meson.current_source_dir(), 'rg.map')
librg = shared_library('rg', librg_sources,
  dependencies: librg_deps,
  link_args: librg_link_args,
  link_depends: 'rg.map',
  version: '0.0.0',
  install: true,
  install_dir: pkglibdir,
)

librg_dep = declare_dependency(
  link_with: librg,
  include_directories: include_directories('.'),
)
