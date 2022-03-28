libide_commands_header_dir = join_paths(libide_header_dir, 'commands')
libide_commands_header_subdir = join_paths(libide_header_subdir, 'commands')
libide_include_directories += include_directories('.')

libide_commands_sources = []
libide_commands_public_headers = []
libide_commands_generated_headers = []

#
# Public API Headers
#

libide_commands_public_headers = [
  'ide-command.h',
  'ide-command-manager.h',
  'ide-command-provider.h',
  'libide-commands.h',
]

libide_commands_private_headers = [
]

install_headers(libide_commands_public_headers, subdir: libide_commands_header_subdir)

#
# Sources
#

libide_commands_public_sources = [
  'ide-command.c',
  'ide-command-manager.c',
  'ide-command-provider.c',
]


libide_commands_private_sources = [
]

libide_commands_sources += libide_commands_public_sources
libide_commands_sources += libide_commands_private_sources

#
# Dependencies
#

libide_commands_deps = [
  libgtk_dep,

  libide_core_dep,
  libide_io_dep,
  libide_threading_dep,
  libide_plugins_dep,
]

#
# Library Definitions
#

libide_commands = static_library('ide-commands-' + libide_api_version,
  libide_commands_sources, libide_commands_generated_headers,
   dependencies: libide_commands_deps,
         c_args: libide_args + release_args + ['-DIDE_COMMANDS_COMPILATION'],
)

libide_commands_dep = declare_dependency(
         dependencies: libide_commands_deps,
            link_with: libide_commands,
  include_directories: include_directories('.'),
              sources: libide_commands_generated_headers,
)

gnome_builder_public_sources += files(libide_commands_public_sources)
gnome_builder_public_headers += files(libide_commands_public_headers)
gnome_builder_private_sources += files(libide_commands_private_sources)
gnome_builder_private_headers += files(libide_commands_private_headers)
gnome_builder_generated_headers += libide_commands_generated_headers
gnome_builder_include_subdirs += libide_commands_header_subdir
gnome_builder_gir_extra_args += ['--c-include=libide-commands.h', '-DIDE_COMMANDS_COMPILATION']
