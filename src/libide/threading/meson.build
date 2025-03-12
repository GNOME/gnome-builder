libide_threading_header_subdir = join_paths(libide_header_subdir, 'threading')
libide_include_directories += include_directories('.')

#
# Public API Headers
#

libide_threading_public_headers = [
  'ide-cancellable.h',
  'ide-environment.h',
  'ide-environment-variable.h',
  'ide-search-path.h',
  'ide-subprocess.h',
  'ide-subprocess-launcher.h',
  'ide-subprocess-supervisor.h',
  'ide-task.h',
  'ide-thread-pool.h',
  'ide-unix-fd-map.h',
  'libide-threading.h',
]

install_headers(libide_threading_public_headers, subdir: libide_threading_header_subdir)

#
# Sources
#

libide_threading_private_headers = [
  'ide-thread-private.h',
  'ide-flatpak-subprocess-private.h',
  'ide-gtask-private.h',
  'ide-simple-subprocess-private.h',
]

libide_threading_private_sources = [
  'ide-flatpak-subprocess.c',
  'ide-simple-subprocess.c',
]

libide_threading_public_sources = [
  'ide-cancellable.c',
  'ide-environment-variable.c',
  'ide-environment.c',
  'ide-search-path.c',
  'ide-gtask.c',
  'ide-subprocess-launcher.c',
  'ide-subprocess-supervisor.c',
  'ide-subprocess.c',
  'ide-task.c',
  'ide-thread-pool.c',
  'ide-unix-fd-map.c',
]

libide_threading_sources = libide_threading_public_sources + libide_threading_private_sources

#
# Library Definitions
#

libide_threading_deps = [
  libgio_dep,
  libgiounix_dep,

  libide_core_dep,
]

libide_threading = static_library('ide-threading-' + libide_api_version, libide_threading_sources,
   dependencies: libide_threading_deps,
         c_args: libide_args + release_args + ['-DIDE_THREADING_COMPILATION'],
)

libide_threading_dep = declare_dependency(
              sources: libide_threading_private_headers,
         dependencies: libide_threading_deps,
            link_with: libide_threading,
  include_directories: include_directories('.'),
)

gnome_builder_public_sources += files(libide_threading_public_sources)
gnome_builder_public_headers += files(libide_threading_public_headers)
gnome_builder_private_sources += files(libide_threading_private_sources)
gnome_builder_private_headers += files(libide_threading_private_headers)
gnome_builder_include_subdirs += libide_threading_header_subdir
gnome_builder_gir_extra_args += '-DIDE_THREADING_COMPILATION'
gnome_builder_main_headers += 'libide-threading.h'
