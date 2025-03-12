libide_terminal_header_subdir = join_paths(libide_header_subdir, 'terminal')
libide_include_directories += include_directories('.')

libide_terminal_generated_headers = []

# Ensure we have PCRE2 available
cc.has_header('pcre2.h', required: true)

#
# Public API Headers
#

libide_terminal_public_headers = [
  'ide-terminal-page.h',
  'ide-terminal-palette.h',
  'ide-terminal-popover.h',
  'ide-terminal-launcher.h',
  'ide-terminal-search.h',
  'ide-terminal-util.h',
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
  'ide-terminal-popover-row.h',
  'ide-terminal-run-command-private.h',
  'ide-terminal-search-private.h',
  'ide-terminal-private.h',
]

libide_terminal_public_sources = [
  'ide-terminal-page.c',
  'ide-terminal-palette.c',
  'ide-terminal-popover.c',
  'ide-terminal-launcher.c',
  'ide-terminal-search.c',
  'ide-terminal-util.c',
  'ide-terminal.c',
]

libide_terminal_private_sources = [
  'ide-terminal-init.c',
  'ide-terminal-page-actions.c',
  'ide-terminal-popover-row.c',
  'ide-terminal-run-command.c',
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
            link_with: libide_terminal,
  include_directories: include_directories('.'),
)

gnome_builder_public_sources += files(libide_terminal_public_sources)
gnome_builder_public_headers += files(libide_terminal_public_headers)
gnome_builder_private_sources += files(libide_terminal_private_sources)
gnome_builder_private_headers += files(libide_terminal_private_headers)
gnome_builder_include_subdirs += libide_terminal_header_subdir
gnome_builder_gir_extra_args += '-DIDE_TERMINAL_COMPILATION'
gnome_builder_main_headers += 'libide-terminal.h'
