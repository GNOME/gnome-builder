libide_themes_header_subdir = join_paths(libide_header_subdir, 'themes')

#
# Sources
#

libide_themes_sources = ['libide-themes.c']
libide_themes_headers = ['libide-themes.h']

#
# Generated Resources
#

libide_themes_resources = gnome.compile_resources(
  'ide-themes-resources',
  'libide-themes.gresource.xml',
  c_name: 'ide_themes',
)

#
# Install Headers
#

install_headers(libide_themes_headers, subdir: libide_themes_header_subdir)

#
# Dependencies
#

libide_themes_deps = [
  libgio_dep,

  libide_core_dep,
]

#
# Library Definitions
#

libide_themes = static_library('ide-themes-' + libide_api_version,
   libide_themes_sources + libide_themes_resources,
   dependencies: libide_themes_deps,
         c_args: libide_args + release_args + ['-DIDE_THEMES_COMPILATION'],
)

libide_themes_dep = declare_dependency(
              sources: libide_themes_resources[1],
         dependencies: libide_themes_deps,
            link_with: libide_themes,
  include_directories: include_directories('.'),
)

gnome_builder_include_subdirs += libide_themes_header_subdir
