libide_io_header_subdir = join_paths(libide_header_subdir, 'io')
libide_include_directories += include_directories('.')

#
# Public API Headers
#

libide_io_public_headers = [
  'ide-content-type.h',
  'ide-gfile.h',
  'ide-line-reader.h',
  'ide-marked-content.h',
  'ide-path.h',
  'ide-persistent-map.h',
  'ide-persistent-map-builder.h',
  'ide-pkcon-transfer.h',
  'ide-pty-intercept.h',
  'libide-io.h',
]

libide_io_private_headers = [
  'ide-gfile-private.h',
]

install_headers(libide_io_public_headers, subdir: libide_io_header_subdir)

#
# Sources
#

libide_io_public_sources = [
  'ide-content-type.c',
  'ide-gfile.c',
  'ide-line-reader.c',
  'ide-marked-content.c',
  'ide-path.c',
  'ide-persistent-map.c',
  'ide-persistent-map-builder.c',
  'ide-pkcon-transfer.c',
  'ide-pty-intercept.c',
]

libide_io_sources = libide_io_public_sources

#
# Dependencies
#

libide_io_deps = [
  libgio_dep,
  libide_core_dep,
  libide_threading_dep
]

#
# Library Definitions
#

libide_io = static_library('ide-io-' + libide_api_version, libide_io_sources,
   dependencies: libide_io_deps,
         c_args: libide_args + release_args + ['-DIDE_IO_COMPILATION'],
)

libide_io_dep = declare_dependency(
         dependencies: [ libgio_dep, libide_core_dep, libide_threading_dep ],
            link_with: libide_io,
  include_directories: include_directories('.'),
)

gnome_builder_public_sources += files(libide_io_public_sources)
gnome_builder_public_headers += files(libide_io_public_headers)
gnome_builder_private_headers += files(libide_io_private_headers)
gnome_builder_include_subdirs += libide_io_header_subdir
gnome_builder_gir_extra_args += ['--c-include=libide-io.h', '-DIDE_IO_COMPILATION']
