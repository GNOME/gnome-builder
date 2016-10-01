libgd_resources = gnome.compile_resources('gd-resources',
  'gd-tagged-entry.gresource.xml',
  c_name: 'gd',
)

libgd_sources = [
  'gd-tagged-entry.c',
  libgd_resources,
]

libgd_deps = [
  libgio_dep,
  libgtk_dep,
]

libgd = static_library('gd', libgd_sources,
  dependencies: libgd_deps,
  pic: true,
)

libgd_dep = declare_dependency(
  link_with: libgd,
  include_directories: include_directories('.'),
)
