libide_docs_header_dir = join_paths(libide_header_dir, 'docs')
libide_docs_header_subdir = join_paths(libide_header_subdir, 'docs')
libide_include_directories += include_directories('.')

#
# Public API Headers
#

libide_docs_public_headers = [
  'libide-docs.h',
  'ide-docs-item.h',
  'ide-docs-library.h',
  'ide-docs-provider.h',
  'ide-docs-query.h',
  'ide-docs-workspace.h',
]

install_headers(libide_docs_public_headers, subdir: libide_docs_header_subdir)

#
# Sources
#

libide_docs_public_sources = [
  'libide-docs.c',
  'ide-docs-item.c',
  'ide-docs-library.c',
  'ide-docs-provider.c',
  'ide-docs-query.c',
  'ide-docs-search-model.c',
  'ide-docs-search-row.c',
  'ide-docs-search-section.c',
  'ide-docs-search-view.c',
  'ide-docs-view.c',
  'ide-docs-workspace.c',
]

libide_docs_sources = libide_docs_public_sources

#
# Enum generation
#

libide_docs_enum_headers = [
  'ide-docs-item.h',
]

libide_docs_enums = gnome.mkenums_simple('ide-docs-enums',
     body_prefix: '#include "config.h"',
   header_prefix: '#include <libide-core.h>',
       decorator: '_IDE_EXTERN',
         sources: libide_docs_enum_headers,
  install_header: true,
     install_dir: libide_docs_header_dir,
)

#
# Generated Resource Files
#

libide_docs_resources = gnome.compile_resources(
  'ide-docs-resources',
  'libide-docs.gresource.xml',
  c_name: 'ide_docs',
)
libide_docs_sources += libide_docs_resources

#
# Dependencies
#

libide_docs_deps = [
  libgio_dep,
  libpeas_dep,
  libide_core_dep,
  libide_threading_dep,
  libide_gui_dep,
]

#
# Library Definitions
#

libide_docs = static_library('ide-docs-' + libide_api_version,
  libide_docs_sources + libide_docs_enums,
   dependencies: libide_docs_deps,
         c_args: libide_args + release_args + ['-DIDE_DOCS_COMPILATION'],
)

libide_docs_dep = declare_dependency(
         dependencies: libide_docs_deps,
           link_whole: libide_docs,
  include_directories: include_directories('.'),
)

gnome_builder_public_sources += files(libide_docs_public_sources)
gnome_builder_public_headers += files(libide_docs_public_headers)
gnome_builder_include_subdirs += libide_docs_header_subdir
gnome_builder_gir_extra_args += ['--c-include=libide-docs.h', '-DIDE_DOCS_COMPILATION']
