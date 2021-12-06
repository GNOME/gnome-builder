libide_sourceview_header_dir = join_paths(libide_header_dir, 'sourceview')
libide_sourceview_header_subdir = join_paths(libide_header_subdir, 'sourceview')
libide_include_directories += include_directories('.')

libide_sourceview_generated_sources = []
libide_sourceview_generated_headers = []

#
# Public API Headers
#

libide_sourceview_private_headers = [
  'ide-completion-list-box.h',
  'ide-completion-overlay.h',
  'ide-completion-private.h',
  'ide-completion-view.h',
  'ide-completion-window.h',
  'ide-cursor.h',
  'ide-hover-popover-private.h',
  'ide-hover-private.h',
  'ide-source-view-capture.h',
  'ide-source-view-mode.h',
  'ide-source-view-movements.h',
  'ide-source-view-private.h',
]

libide_sourceview_public_headers = [
  'ide-completion-context.h',
  'ide-completion-display.h',
  'ide-completion-list-box-row.h',
  'ide-completion-proposal.h',
  'ide-completion-provider.h',
  'ide-completion-types.h',
  'ide-completion.h',
  'ide-gutter.h',
  'ide-hover-context.h',
  'ide-hover-provider.h',
  'ide-indenter.h',
  'ide-line-change-gutter-renderer.h',
  'ide-snippet-chunk.h',
  'ide-snippet-context.h',
  'ide-snippet-parser.h',
  'ide-snippet-storage.h',
  'ide-snippet-types.h',
  'ide-snippet.h',
  'ide-source-search-context.h',
  'ide-source-view.h',
  'ide-text-util.h',
  'libide-sourceview.h',
]

libide_sourceview_enum_headers = [
  'ide-completion-types.h',
  'ide-source-view.h',
]

install_headers(libide_sourceview_public_headers, subdir: libide_sourceview_header_subdir)

#
# Sources
#

libide_sourceview_private_sources = [
  'ide-completion-list-box.c',
  'ide-completion-overlay.c',
  'ide-completion-view.c',
  'ide-completion-window.c',
  'ide-cursor.c',
  'ide-hover.c',
  'ide-hover-popover.c',
  'ide-line-change-gutter-renderer.c',
  'ide-source-view-capture.c',
  'ide-source-view-mode.c',
  'ide-source-view-movements.c',
  'ide-source-view-shortcuts.c',
  'ide-text-util.c',
]

libide_sourceview_public_sources = [
  'ide-completion-proposal.c',
  'ide-completion-provider.c',
  'ide-completion-context.c',
  'ide-completion-display.c',
  'ide-completion-list-box-row.c',
  'ide-completion.c',
  'ide-gutter.c',
  'ide-hover-context.c',
  'ide-hover-provider.c',
  'ide-indenter.c',
  'ide-source-search-context.c',
  'ide-source-view.c',
  'ide-snippet.c',
  'ide-snippet-chunk.c',
  'ide-snippet-context.c',
  'ide-snippet-parser.c',
  'ide-snippet-storage.c',
]

#
# Generated Resource Files
#

libide_sourceview_resources = gnome.compile_resources(
  'ide-sourceview-resources',
  'libide-sourceview.gresource.xml',
  c_name: 'ide_sourceview',
)
libide_sourceview_generated_headers += [libide_sourceview_resources[1]]
libide_sourceview_generated_sources += libide_sourceview_resources

#
# Enum generation
#

libide_sourceview_enums = gnome.mkenums_simple('ide-source-view-enums',
     body_prefix: '#include "config.h"',
   header_prefix: '#include <libide-core.h>',
       decorator: '_IDE_EXTERN',
         sources: libide_sourceview_enum_headers,
  install_header: true,
     install_dir: libide_sourceview_header_dir,
)
libide_sourceview_generated_sources += [libide_sourceview_enums[0]]
libide_sourceview_generated_headers += [libide_sourceview_enums[1]]


#
# Dependencies
#

libide_sourceview_deps = [
  libgio_dep,
  libgtk_dep,
  libgtksource_dep,
  libdazzle_dep,

  libide_core_dep,
  libide_threading_dep,
  libide_code_dep,
  libide_plugins_dep,
  libide_io_dep,
  libide_gui_dep,
]

#
# Library Definitions
#

libide_sourceview = static_library('ide-sourceview-' + libide_api_version,
                                   libide_sourceview_public_sources,
                                   libide_sourceview_private_sources,
                                   libide_sourceview_generated_sources,
                                   libide_sourceview_generated_headers,
   dependencies: libide_sourceview_deps,
         c_args: libide_args + release_args + ['-DIDE_SOURCEVIEW_COMPILATION'],
)

libide_sourceview_dep = declare_dependency(
              sources: libide_sourceview_private_headers + libide_sourceview_generated_headers,
         dependencies: libide_sourceview_deps,
            link_with: libide_sourceview,
  include_directories: include_directories('.'),
)

gnome_builder_public_sources += files(libide_sourceview_public_sources)
gnome_builder_public_headers += files(libide_sourceview_public_headers)
gnome_builder_private_sources += files(libide_sourceview_private_sources)
gnome_builder_private_headers += files(libide_sourceview_private_headers)
gnome_builder_generated_headers += libide_sourceview_generated_headers
gnome_builder_generated_sources += libide_sourceview_generated_sources
gnome_builder_include_subdirs += libide_sourceview_header_subdir
gnome_builder_gir_extra_args += ['--c-include=libide-sourceview.h', '-DIDE_SOURCEVIEW_COMPILATION']
