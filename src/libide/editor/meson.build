libide_editor_header_dir = join_paths(libide_header_dir, 'editor')
libide_editor_header_subdir = join_paths(libide_header_subdir, 'editor')
libide_include_directories += include_directories('.')

libide_editor_sources = []
libide_editor_public_headers = []
libide_editor_generated_headers = []

#
# Public API Headers
#

libide_editor_public_headers = [
  'ide-editor-addin.h',
  'ide-editor-page.h',
  'ide-editor-page-addin.h',
  'ide-editor-search.h',
  'ide-editor-sidebar.h',
  'ide-editor-surface.h',
  'ide-editor-utilities.h',
  'ide-editor-workspace.h',
  'libide-editor.h',
]

libide_editor_private_headers = [
  'ide-editor-print-operation.h',
  'ide-editor-search-bar.h',
  'ide-editor-settings-dialog.h',
]

install_headers(libide_editor_public_headers, subdir: libide_editor_header_subdir)

#
# Sources
#

libide_editor_public_sources = [
  'ide-editor-addin.c',
  'ide-editor-page.c',
  'ide-editor-page-addin.c',
  'ide-editor-search.c',
  'ide-editor-sidebar.c',
  'ide-editor-surface.c',
  'ide-editor-utilities.c',
  'ide-editor-workspace.c',
]


libide_editor_private_sources = [
  'ide-editor-page-actions.c',
  'ide-editor-page-settings.c',
  'ide-editor-page-shortcuts.c',
  'ide-editor-print-operation.c',
  'ide-editor-search-bar.c',
  'ide-editor-search-bar-shortcuts.c',
  'ide-editor-settings-dialog.c',
  'ide-editor-surface-actions.c',
  'ide-editor-surface-shortcuts.c',
]

libide_editor_sources += libide_editor_public_sources
libide_editor_sources += libide_editor_private_sources

#
# Generated Resource Files
#

libide_editor_resources = gnome.compile_resources(
  'ide-editor-resources',
  'libide-editor.gresource.xml',
  c_name: 'ide_editor',
)
libide_editor_generated_headers += [libide_editor_resources[1]]
libide_editor_sources += libide_editor_resources

#
# Dependencies
#

libide_editor_deps = [
  libgio_dep,
  libgtk_dep,
  libdazzle_dep,
  libpeas_dep,

  libide_core_dep,
  libide_io_dep,
  libide_projects_dep,
  libide_search_dep,
  libide_sourceview_dep,
  libide_threading_dep,
  libide_gui_dep,
]

libide_editor_internal_deps = [
  libpangoft2_dep,
]

#
# Library Definitions
#

libide_editor = static_library('ide-editor-' + libide_api_version, libide_editor_sources,
   dependencies: libide_editor_deps + libide_editor_internal_deps,
         c_args: libide_args + release_args + ['-DIDE_EDITOR_COMPILATION'],
)

libide_editor_dep = declare_dependency(
         dependencies: libide_editor_deps,
            link_with: libide_editor,
  include_directories: include_directories('.'),
              sources: libide_editor_generated_headers,
)

gnome_builder_public_sources += files(libide_editor_public_sources)
gnome_builder_public_headers += files(libide_editor_public_headers)
gnome_builder_private_sources += files(libide_editor_private_sources)
gnome_builder_private_headers += files(libide_editor_private_headers)
gnome_builder_generated_headers += libide_editor_generated_headers
gnome_builder_include_subdirs += libide_editor_header_subdir
gnome_builder_gir_extra_args += ['--c-include=libide-editor.h', '-DIDE_EDITOR_COMPILATION']
