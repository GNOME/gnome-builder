libide_greeter_header_subdir = join_paths(libide_header_subdir, 'greeter')
libide_include_directories += include_directories('.')

libide_greeter_generated_headers = []

#
# Public API Headers
#

libide_greeter_public_headers = [
  'ide-clone-surface.h',
  'ide-greeter-row.h',
  'ide-greeter-section.h',
  'ide-greeter-workspace.h',
  'libide-greeter.h',
]

libide_greeter_private_headers = [
  'ide-greeter-buttons-section.h',
  'ide-greeter-private.h',
  'ide-truncate-model.h',
]

install_headers(libide_greeter_public_headers, subdir: libide_greeter_header_subdir)

#
# Sources
#

libide_greeter_public_sources = [
  'ide-clone-surface.c',
  'ide-greeter-row.c',
  'ide-greeter-section.c',
  'ide-greeter-workspace.c',
]

libide_greeter_private_sources = [
  'ide-greeter-workspace-actions.c',
  'ide-greeter-workspace-shortcuts.c',
  'ide-greeter-buttons-section.c',
  'ide-truncate-model.c',
]

#
# Generated Resource Files
#

libide_greeter_resources = gnome.compile_resources(
  'ide-greeter-resources',
  'libide-greeter.gresource.xml',
  c_name: 'ide_greeter',
)
libide_greeter_generated_headers += [libide_greeter_resources[1]]
libide_greeter_private_sources += libide_greeter_resources


#
# Dependencies
#

libide_greeter_deps = [
  libgio_dep,
  libgtk_dep,
  libdazzle_dep,

  libide_core_dep,
  libide_gui_dep,
  libide_io_dep,
  libide_threading_dep,
  libide_vcs_dep,
]

#
# Library Definitions
#

libide_greeter = static_library('ide-greeter-' + libide_api_version,
   libide_greeter_public_sources + libide_greeter_private_sources,
   dependencies: libide_greeter_deps,
         c_args: libide_args + release_args + ['-DIDE_GREETER_COMPILATION'],
)

libide_greeter_dep = declare_dependency(
              sources: libide_greeter_private_headers + libide_greeter_generated_headers,
         dependencies: libide_greeter_deps,
            link_with: libide_greeter,
  include_directories: include_directories('.'),
)

gnome_builder_public_sources += files(libide_greeter_public_sources)
gnome_builder_public_headers += files(libide_greeter_public_headers)
gnome_builder_private_headers += files(libide_greeter_private_headers)
gnome_builder_generated_headers += libide_greeter_generated_headers
gnome_builder_include_subdirs += libide_greeter_header_subdir
gnome_builder_gir_extra_args += ['--c-include=libide-greeter.h', '-DIDE_GREETER_COMPILATION']
