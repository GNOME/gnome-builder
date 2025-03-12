libide_vcs_header_dir = join_paths(libide_header_dir, 'vcs')
libide_vcs_header_subdir = join_paths(libide_header_subdir, 'vcs')
libide_include_directories += include_directories('.')

#
# Public API Headers
#

libide_vcs_public_headers = [
  'ide-directory-vcs.h',
  'ide-vcs-branch.h',
  'ide-vcs-config.h',
  'ide-vcs-cloner.h',
  'ide-vcs-clone-request.h',
  'ide-vcs-file-info.h',
  'ide-vcs-initializer.h',
  'ide-vcs-monitor.h',
  'ide-vcs-uri.h',
  'ide-vcs.h',
  'ide-vcs-tag.h',
  'libide-vcs.h',
]

libide_vcs_enum_headers = [
  'ide-vcs-config.h',
  'ide-vcs-file-info.h',
]

install_headers(libide_vcs_public_headers, subdir: libide_vcs_header_subdir)

#
# Sources
#

libide_vcs_public_sources = [
  'ide-directory-vcs.c',
  'ide-vcs-branch.c',
  'ide-vcs-config.c',
  'ide-vcs-cloner.c',
  'ide-vcs-clone-request.c',
  'ide-vcs-file-info.c',
  'ide-vcs-initializer.c',
  'ide-vcs-monitor.c',
  'ide-vcs-tag.c',
  'ide-vcs-uri.c',
  'ide-vcs.c',
]

#
# Enum generation
#

libide_vcs_enums = gnome.mkenums_simple('ide-vcs-enums',
     body_prefix: '#include "config.h"',
   header_prefix: '#include <libide-core.h>',
       decorator: '_IDE_EXTERN',
         sources: libide_vcs_enum_headers,
  install_header: true,
     install_dir: libide_vcs_header_dir,
)
libide_vcs_generated_sources = [libide_vcs_enums[0]]
libide_vcs_generated_headers = [libide_vcs_enums[1]]

#
# Dependencies
#

libide_vcs_deps = [
  libgio_dep,
  libgtk_dep,

  libide_core_dep,
  libide_io_dep,
  libide_plugins_dep,
  libide_threading_dep,
]

#
# Library Definitions
#

libide_vcs = static_library('ide-vcs-' + libide_api_version,
   libide_vcs_public_sources + libide_vcs_generated_sources + libide_vcs_generated_headers,
   dependencies: libide_vcs_deps,
         c_args: libide_args + release_args + ['-DIDE_VCS_COMPILATION'],
)

libide_vcs_dep = declare_dependency(
         dependencies: libide_vcs_deps,
            link_with: libide_vcs,
  include_directories: include_directories('.'),
              sources: libide_vcs_generated_headers,
)

gnome_builder_public_sources += files(libide_vcs_public_sources)
gnome_builder_public_headers += files(libide_vcs_public_headers)
gnome_builder_generated_headers += libide_vcs_generated_headers
gnome_builder_generated_sources += libide_vcs_generated_sources
gnome_builder_include_subdirs += libide_vcs_header_subdir
gnome_builder_gir_extra_args += '-DIDE_VCS_COMPILATION'
gnome_builder_main_headers += 'libide-vcs.h'
