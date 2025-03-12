libide_debugger_header_subdir = join_paths(libide_header_subdir, 'debugger')
libide_include_directories += include_directories('.')

libide_debugger_generated_headers = []

#
# Public API Headers
#

libide_debugger_public_headers = [
  'ide-debug-manager.h',
  'ide-debugger-breakpoint.h',
  'ide-debugger-breakpoints.h',
  'ide-debugger-frame.h',
  'ide-debugger-instruction.h',
  'ide-debugger-library.h',
  'ide-debugger-register.h',
  'ide-debugger-thread-group.h',
  'ide-debugger-thread.h',
  'ide-debugger-types.h',
  'ide-debugger-variable.h',
  'ide-debugger.h',
  'libide-debugger.h',
]

libide_debugger_private_headers = [
  'ide-debugger-address-map-private.h',
  'ide-debugger-private.h',
]

install_headers(libide_debugger_public_headers, subdir: libide_debugger_header_subdir)

#
# Sources
#

libide_debugger_public_sources = [
  'ide-debug-manager.c',
  'ide-debugger-address-map.c',
  'ide-debugger-breakpoint.c',
  'ide-debugger-breakpoints.c',
  'ide-debugger-frame.c',
  'ide-debugger-instruction.c',
  'ide-debugger-library.c',
  'ide-debugger-register.c',
  'ide-debugger-thread-group.c',
  'ide-debugger-thread.c',
  'ide-debugger-types.c',
  'ide-debugger-variable.c',
  'ide-debugger.c',
]

libide_debugger_private_sources = [
  'ide-debugger-fallbacks.c',
  'ide-debugger-actions.c',
]

#
# Dependencies
#

libide_debugger_deps = [
  libgio_dep,
  libgtk_dep,

  libide_core_dep,
  libide_io_dep,
  libide_threading_dep,
  libide_code_dep,
  libide_foundry_dep,
]

#
# Library Definitions
#

libide_debugger = static_library('ide-debugger-' + libide_api_version,
   libide_debugger_public_sources + libide_debugger_private_sources,
   dependencies: libide_debugger_deps,
         c_args: libide_args + release_args + ['-DIDE_DEBUGGER_COMPILATION'],
)

libide_debugger_dep = declare_dependency(
              sources: libide_debugger_private_headers + libide_debugger_generated_headers,
         dependencies: libide_debugger_deps,
            link_with: libide_debugger,
  include_directories: include_directories('.'),
)

gnome_builder_public_sources += files(libide_debugger_public_sources)
gnome_builder_public_headers += files(libide_debugger_public_headers)
gnome_builder_generated_headers += libide_debugger_generated_headers
gnome_builder_include_subdirs += libide_debugger_header_subdir
gnome_builder_gir_extra_args += '-DIDE_DEBUGGER_COMPILATION'
gnome_builder_main_headers += 'libide-debugger.h'
