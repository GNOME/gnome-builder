libide_tree_header_subdir = join_paths(libide_header_subdir, 'tree')
libide_include_directories += include_directories('.')

#
# Public API Headers
#

libide_tree_public_headers = [
  'ide-tree.h',
  'ide-tree-addin.h',
  'ide-tree-model.h',
  'ide-tree-node.h',
  'libide-tree.h',
]

install_headers(libide_tree_public_headers, subdir: libide_tree_header_subdir)

#
# Sources
#

libide_tree_public_sources = [
  'ide-tree.c',
  'ide-tree-addin.c',
  'ide-tree-model.c',
  'ide-tree-node.c',
]

libide_tree_private_headers = [
  'ide-cell-renderer-status.h',
]

libide_tree_private_sources = [
  'ide-cell-renderer-status.c',
]

libide_tree_sources = libide_tree_public_sources + libide_tree_private_sources

#
# Dependencies
#

libide_tree_deps = [
  libgtk_dep,
  libpeas_dep,

  libide_core_dep,
  libide_plugins_dep,
  libide_threading_dep,
]

#
# Library Definitions
#

libide_tree = static_library('ide-tree-' + libide_api_version, libide_tree_sources,
   dependencies: libide_tree_deps,
         c_args: libide_args + release_args + ['-DIDE_TREE_COMPILATION'],
)

libide_tree_dep = declare_dependency(
              sources: libide_tree_private_headers,
         dependencies: libide_tree_deps,
            link_with: libide_tree,
  include_directories: include_directories('.'),
)

gnome_builder_public_sources += files(libide_tree_public_sources)
gnome_builder_public_headers += files(libide_tree_public_headers)
gnome_builder_private_sources += files(libide_tree_private_sources)
gnome_builder_private_headers += files(libide_tree_private_headers)
gnome_builder_include_subdirs += libide_tree_header_subdir
gnome_builder_gir_extra_args += ['--c-include=libide-tree.h', '-DIDE_TREE_COMPILATION']
