libide_tree_header_dir = join_paths(libide_header_dir, 'tree')
libide_tree_header_subdir = join_paths(libide_header_subdir, 'tree')
libide_include_directories += include_directories('.')

#
# Public API Headers
#

libide_tree_public_headers = [
  'ide-tree.h',
  'ide-tree-addin.h',
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
  'ide-tree-node.c',
]

libide_tree_private_sources = [
  'ide-tree-empty.c',
]

libide_tree_sources = libide_tree_public_sources + libide_tree_private_sources

#
# Enum generation
#

libide_tree_enum_headers = [
  'ide-tree-node.h',
]

libide_tree_enums = gnome.mkenums_simple('ide-tree-enums',
     body_prefix: '#include "config.h"',
   header_prefix: '#include <libide-core.h>',
       decorator: '_IDE_EXTERN',
         sources: libide_tree_enum_headers,
  install_header: true,
     install_dir: libide_tree_header_dir,
)
libide_tree_generated_sources = [libide_tree_enums[0]]
libide_tree_generated_headers = [libide_tree_enums[1]]

#
# Generated Resource Files
#

libide_tree_resources = gnome.compile_resources(
  'ide-tree-resources',
  'libide-tree.gresource.xml',
  c_name: 'ide_tree',
)
libide_tree_generated_headers += [libide_tree_resources[1]]
libide_tree_generated_sources += [libide_tree_resources[0]]

#
# Dependencies
#

libide_tree_deps = [
  libgtk_dep,
  libpeas_dep,

  libide_core_dep,
  libide_gtk_dep,
  libide_plugins_dep,
  libide_threading_dep,
]

#
# Library Definitions
#

libide_tree = static_library('ide-tree-' + libide_api_version,
  libide_tree_sources + libide_tree_enums + libide_tree_resources,
   dependencies: libide_tree_deps,
         c_args: libide_args + release_args + ['-DIDE_TREE_COMPILATION'],
)

libide_tree_dep = declare_dependency(
         dependencies: libide_tree_deps,
            link_with: libide_tree,
  include_directories: include_directories('.'),
)

gnome_builder_public_sources += files(libide_tree_public_sources)
gnome_builder_public_headers += files(libide_tree_public_headers)
gnome_builder_generated_headers += libide_tree_generated_headers
gnome_builder_generated_sources += libide_tree_generated_sources
gnome_builder_include_subdirs += libide_tree_header_subdir
gnome_builder_gir_extra_args += '-DIDE_TREE_COMPILATION'
gnome_builder_main_headers += 'libide-tree.h'
