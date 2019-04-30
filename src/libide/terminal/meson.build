libide_terminal_header_subdir = join_paths(libide_header_subdir, 'terminal')
libide_include_directories += include_directories('.')

libide_terminal_generated_headers = []

#
# Public API Headers
#

libide_terminal_public_headers = [
  'ide-terminal-page.h',
  'ide-terminal-launcher.h',
  'ide-terminal-search.h',
  'ide-terminal-surface.h',
  'ide-terminal-util.h',
  'ide-terminal-workspace.h',
  'ide-terminal.h',
  'libide-terminal.h',
]

install_headers(libide_terminal_public_headers, subdir: libide_terminal_header_subdir)

#
# Sources
#

libide_terminal_private_headers = [
  'ide-terminal-page-actions.h',
  'ide-terminal-page-private.h',
  'ide-terminal-private.h',
  'ide-terminal-search-private.h',
]

libide_terminal_public_sources = [
  'ide-terminal-page.c',
  'ide-terminal-launcher.c',
  'ide-terminal-search.c',
  'ide-terminal-surface.c',
  'ide-terminal-util.c',
  'ide-terminal-workspace.c',
  'ide-terminal.c',
]

libide_terminal_private_sources = [
  'ide-terminal-page-actions.c',
]

#
# Generated Resource Files
#

libide_terminal_resources = gnome.compile_resources(
  'ide-terminal-resources',
  'libide-terminal.gresource.xml',
  c_name: 'ide_terminal',
)
libide_terminal_generated_headers += [libide_terminal_resources[1]]


#
# Dependencies
#

libide_terminal_deps = [
  libgio_dep,
  libgtk_dep,
  libdazzle_dep,
  libvte_dep,

  libide_core_dep,
  libide_io_dep,
  libide_threading_dep,
  libide_gui_dep,
]

#
# Library Definitions
#

libide_terminal = static_library('ide-terminal-' + libide_api_version,
   libide_terminal_public_sources + libide_terminal_private_sources + libide_terminal_resources,
   dependencies: libide_terminal_deps,
         c_args: libide_args + release_args + ['-DIDE_TERMINAL_COMPILATION'],
)

libide_terminal_dep = declare_dependency(
              sources: libide_terminal_generated_headers,
         dependencies: libide_terminal_deps,
           link_whole: libide_terminal,
  include_directories: include_directories('.'),
)

gnome_builder_public_sources += files(libide_terminal_public_sources)
gnome_builder_public_headers += files(libide_terminal_public_headers)
gnome_builder_private_sources += files(libide_terminal_private_sources)
gnome_builder_private_headers += files(libide_terminal_private_headers)
gnome_builder_include_subdirs += libide_terminal_header_subdir
gnome_builder_gir_extra_args += ['--c-include=libide-terminal.h', '-DIDE_TERMINAL_COMPILATION']
