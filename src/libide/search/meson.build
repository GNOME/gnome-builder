libide_search_header_dir = join_paths(libide_header_dir, 'search')
libide_search_header_subdir = join_paths(libide_header_subdir, 'search')
libide_include_directories += include_directories('.')

#
# Public API Headers
#

libide_search_public_headers = [
  'ide-fuzzy-index-builder.h',
  'ide-fuzzy-index-cursor.h',
  'ide-fuzzy-index.h',
  'ide-fuzzy-index-match.h',
  'ide-fuzzy-mutable-index.h',
  'ide-pattern-spec.h',
  'ide-search-engine.h',
  'ide-search-preview.h',
  'ide-search-provider.h',
  'ide-search-reducer.h',
  'ide-search-result.h',
  'ide-search-results.h',
  'libide-search.h',
]

install_headers(libide_search_public_headers, subdir: libide_search_header_subdir)

#
# Sources
#

libide_search_public_sources = [
  'ide-fuzzy-index-builder.c',
  'ide-fuzzy-index.c',
  'ide-fuzzy-index-cursor.c',
  'ide-fuzzy-index-match.c',
  'ide-fuzzy-mutable-index.c',
  'ide-pattern-spec.c',
  'ide-search-engine.c',
  'ide-search-preview.c',
  'ide-search-provider.c',
  'ide-search-reducer.c',
  'ide-search-result.c',
  'ide-search-results.c',
]

libide_search_private_sources = [
  'ide-search-init.c',
]

libide_search_sources = libide_search_public_sources + libide_search_private_sources

#
# Generated Resource Files
#

libide_search_resources = gnome.compile_resources(
  'ide-search-resources',
  'libide-search.gresource.xml',
  c_name: 'ide_search',
)
libide_search_sources += libide_search_resources

#
# Enum generation
#

libide_search_enum_headers = [
  'ide-search-provider.h',
]
libide_search_enums = gnome.mkenums_simple('ide-search-enums',
     body_prefix: '#include "config.h"',
   header_prefix: '#include <libide-core.h>',
       decorator: '_IDE_EXTERN',
         sources: libide_search_enum_headers,
  install_header: true,
     install_dir: libide_search_header_dir,
)
libide_search_generated_sources = [libide_search_enums[0]]
libide_search_generated_headers = [libide_search_enums[1]]

#
# Dependencies
#

libide_search_deps = [
  libadwaita_dep,
  libgio_dep,
  libgtksource_dep,
  libpeas_dep,

  libide_core_dep,
  libide_gtk_dep,
  libide_threading_dep,
  libide_plugins_dep
]

#
# Library Definitions
#

libide_search = static_library('ide-search-' + libide_api_version,
  libide_search_sources + libide_search_enums,
   dependencies: libide_search_deps,
         c_args: libide_args + release_args + ['-DIDE_SEARCH_COMPILATION'],
)

libide_search_dep = declare_dependency(
         dependencies: libide_search_deps,
            link_with: libide_search,
  include_directories: include_directories('.'),
)

gnome_builder_public_sources += files(libide_search_public_sources)
gnome_builder_public_headers += files(libide_search_public_headers)
gnome_builder_generated_headers += libide_search_generated_headers
gnome_builder_generated_sources += libide_search_generated_sources
gnome_builder_include_subdirs += libide_search_header_subdir
gnome_builder_gir_extra_args += '-DIDE_SEARCH_COMPILATION'
gnome_builder_main_headers += 'libide-search.h'
