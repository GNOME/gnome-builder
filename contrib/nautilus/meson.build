libnautilus_sources = [
  'nautilus-floating-bar.c',
]

libnautilus_deps = [
  libgio_dep,
  libgtk_dep,
]

libnautilus = static_library('nautilus', libnautilus_sources,
  dependencies: libnautilus_deps,
  pic: true,
)

libnautilus_dep = declare_dependency(
  link_with: libnautilus,
  include_directories: include_directories('.'),
)
