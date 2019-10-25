libide_search_header_subdir = join_paths(libide_header_subdir, 'search')
libide_include_directories += include_directories('.')

#
# Public API Headers
#

libide_search_public_headers = [
  'ide-search-engine.h',
  'ide-search-provider.h',
  'ide-search-reducer.h',
  'ide-search-result.h',
  'libide-search.h',
]

install_headers(libide_search_public_headers, subdir: libide_search_header_subdir)

#
# Sources
#

libide_search_public_sources = [
  'ide-search-engine.c',
  'ide-search-provider.c',
  'ide-search-reducer.c',
  'ide-search-result.c',
]

libide_search_sources = libide_search_public_sources

#
# Dependencies
#

libide_search_deps = [
  libgio_dep,
  libdazzle_dep,
  libpeas_dep,
  libide_core_dep,
  libide_threading_dep,
]

#
# Library Definitions
#

libide_search = static_library('ide-search-' + libide_api_version, libide_search_sources,
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
gnome_builder_include_subdirs += libide_search_header_subdir
gnome_builder_gir_extra_args += ['--c-include=libide-search.h', '-DIDE_SEARCH_COMPILATION']
