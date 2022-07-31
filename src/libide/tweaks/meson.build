libide_tweaks_header_dir = join_paths(libide_header_dir, 'tweaks')
libide_tweaks_header_subdir = join_paths(libide_header_subdir, 'tweaks')
libide_include_directories += include_directories('.')

#
# Public API Headers
#

libide_tweaks_public_headers = [
  'libide-tweaks.h',
  'ide-tweaks.h',
  'ide-tweaks-custom.h',
  'ide-tweaks-group.h',
  'ide-tweaks-item.h',
  'ide-tweaks-page.h',
  'ide-tweaks-subpage.h',
  'ide-tweaks-subpage-generator.h',
  'ide-tweaks-variable.h',
]

install_headers(libide_tweaks_public_headers, subdir: libide_tweaks_header_subdir)

#
# Sources
#

libide_tweaks_public_sources = [
  'ide-tweaks.c',
  'ide-tweaks-custom.c',
  'ide-tweaks-item.c',
  'ide-tweaks-group.c',
  'ide-tweaks-page.c',
  'ide-tweaks-subpage.c',
  'ide-tweaks-subpage-generator.c',
  'ide-tweaks-variable.c',
]

libide_tweaks_private_sources = [
  'ide-tweaks-init.c',
]

#
# Dependencies
#

libide_tweaks_deps = [
  libgio_dep,
  libgtk_dep,

  libide_core_dep,
]

#
# Library Definitions
#

libide_tweaks = static_library('ide-tweaks-' + libide_api_version,
   libide_tweaks_public_sources + libide_tweaks_private_sources,
   dependencies: libide_tweaks_deps,
         c_args: libide_args + release_args + ['-DIDE_TWEAKS_COMPILATION'],
)

libide_tweaks_dep = declare_dependency(
         dependencies: libide_tweaks_deps,
            link_with: libide_tweaks,
  include_directories: include_directories('.'),
)

gnome_builder_public_sources += files(libide_tweaks_public_sources)
gnome_builder_public_headers += files(libide_tweaks_public_headers)
gnome_builder_include_subdirs += libide_tweaks_header_subdir
gnome_builder_gir_extra_args += ['--c-include=libide-tweaks.h', '-DIDE_TWEAKS_COMPILATION']

test_tweaks = executable('test-tweaks', 'test-tweaks.c',
  dependencies: [libide_tweaks_dep],
)
