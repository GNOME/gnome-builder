libide_lsp_header_dir = join_paths(libide_header_dir, 'lsp')
libide_lsp_header_subdir = join_paths(libide_header_subdir, 'lsp')
libide_include_directories += include_directories('.')

#
# Public API Headers
#

libide_lsp_public_headers = [
  'libide-lsp.h',
  'ide-lsp-client.h',
  'ide-lsp-code-action.h',
  'ide-lsp-code-action-provider.h',
  'ide-lsp-completion-item.h',
  'ide-lsp-completion-provider.h',
  'ide-lsp-completion-results.h',
  'ide-lsp-diagnostic.h',
  'ide-lsp-diagnostic-provider.h',
  'ide-lsp-formatter.h',
  'ide-lsp-highlighter.h',
  'ide-lsp-hover-provider.h',
  'ide-lsp-plugin.h',
  'ide-lsp-rename-provider.h',
  'ide-lsp-search-provider.h',
  'ide-lsp-service.h',
  'ide-lsp-symbol-node.h',
  'ide-lsp-symbol-resolver.h',
  'ide-lsp-symbol-tree.h',
  'ide-lsp-types.h',
  'ide-lsp-util.h',
  'ide-lsp-workspace-edit.h',
]

libide_lsp_private_headers = [
  'ide-lsp-plugin-private.h',
  'ide-lsp-symbol-node-private.h',
  'ide-lsp-symbol-tree-private.h',
]

install_headers(libide_lsp_public_headers, subdir: libide_lsp_header_subdir)

#
# Sources
#

libide_lsp_public_sources = [
  'ide-lsp-client.c',
  'ide-lsp-code-action.c',
  'ide-lsp-code-action-provider.c',
  'ide-lsp-completion-item.c',
  'ide-lsp-completion-provider.c',
  'ide-lsp-completion-results.c',
  'ide-lsp-diagnostic.c',
  'ide-lsp-diagnostic-provider.c',
  'ide-lsp-formatter.c',
  'ide-lsp-highlighter.c',
  'ide-lsp-hover-provider.c',
  'ide-lsp-plugin.c',
  'ide-lsp-rename-provider.c',
  'ide-lsp-search-provider.c',
  'ide-lsp-search-result.c',
  'ide-lsp-service.c',
  'ide-lsp-symbol-node.c',
  'ide-lsp-symbol-resolver.c',
  'ide-lsp-symbol-tree.c',
  'ide-lsp-workspace-edit.c',
  'ide-lsp-util.c',
]

libide_lsp_private_sources = [
  'ide-lsp-plugin-code-action-provider.c',
  'ide-lsp-plugin-completion-provider.c',
  'ide-lsp-plugin-diagnostic-provider.c',
  'ide-lsp-plugin-formatter.c',
  'ide-lsp-plugin-highlighter.c',
  'ide-lsp-plugin-hover-provider.c',
  'ide-lsp-plugin-rename-provider.c',
  'ide-lsp-plugin-search-provider.c',
  'ide-lsp-plugin-symbol-resolver.c',
]

libide_lsp_enum_headers = [
  'ide-lsp-client.h',
]

libide_lsp_enums = gnome.mkenums_simple('ide-lsp-enums',
     body_prefix: '#include "config.h"',
   header_prefix: '#include <libide-core.h>',
       decorator: '_IDE_EXTERN',
         sources: libide_lsp_enum_headers,
  install_header: true,
     install_dir: libide_lsp_header_dir,
)
libide_lsp_generated_sources = [libide_lsp_enums[0]]
libide_lsp_generated_headers = [libide_lsp_enums[1]]

libide_lsp_sources = libide_lsp_public_sources + libide_lsp_private_sources + libide_lsp_generated_sources

#
# Dependencies
#

libide_lsp_deps = [
  libgio_dep,
  libjson_glib_dep,
  libjsonrpc_glib_dep,

  libide_code_dep,
  libide_core_dep,
  libide_io_dep,
  libide_foundry_dep,
  libide_projects_dep,
  libide_sourceview_dep,
  libide_threading_dep,
  libide_editor_dep,
]

#
# Library Definitions
#

libide_lsp = static_library('ide-lsp-' + libide_api_version, libide_lsp_sources, libide_lsp_generated_headers,
   dependencies: libide_lsp_deps,
         c_args: libide_args + release_args + ['-DIDE_LSP_COMPILATION'],
)

libide_lsp_dep = declare_dependency(
              sources: libide_lsp_private_headers,
         dependencies: libide_lsp_deps,
            link_with: libide_lsp,
  include_directories: include_directories('.'),
)

gnome_builder_public_sources += files(libide_lsp_public_sources)
gnome_builder_public_headers += files(libide_lsp_public_headers)
gnome_builder_include_subdirs += libide_lsp_header_subdir
gnome_builder_gir_extra_args += '-DIDE_LSP_COMPILATION'
gnome_builder_main_headers += 'libide-lsp.h'
