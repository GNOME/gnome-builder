libide_plugins_header_subdir = join_paths(libide_header_subdir, 'plugins')
libide_include_directories += include_directories('.')

#
# Public API Headers
#

libide_plugins_public_headers = [
  'ide-extension-adapter.h',
  'ide-extension-set-adapter.h',
  'libide-plugins.h',
]

libide_plugins_private_headers = [
  'ide-extension-util-private.h',
]

install_headers(libide_plugins_public_headers, subdir: libide_plugins_header_subdir)

#
# Sources
#

libide_plugins_public_sources = [
  'ide-extension-adapter.c',
  'ide-extension-set-adapter.c',
]

libide_plugins_private_sources = [
  'ide-extension-util.c',
]

#
# Library Definitions
#

libide_plugins_deps = [
  libgio_dep,
  libpeas_dep,
  libdazzle_dep,

  libide_core_dep,
]

libide_plugins = static_library('ide-plugins-' + libide_api_version,
  libide_plugins_public_sources, libide_plugins_private_sources,
   dependencies: libide_plugins_deps,
         c_args: libide_args + release_args + ['-DIDE_PLUGINS_COMPILATION'],
)

libide_plugins_dep = declare_dependency(
              sources: libide_plugins_private_headers,
         dependencies: libide_plugins_deps,
            link_with: libide_plugins,
  include_directories: include_directories('.'),
)

gnome_builder_public_sources += files(libide_plugins_public_sources)
gnome_builder_public_headers += files(libide_plugins_public_headers)
gnome_builder_include_subdirs += libide_plugins_header_subdir
gnome_builder_gir_extra_args += ['--c-include=libide-plugins.h', '-DIDE_PLUGINS_COMPILATION']
