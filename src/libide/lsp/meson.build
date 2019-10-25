libide_lsp_header_subdir = join_paths(libide_header_subdir, 'lsp')
libide_include_directories += include_directories('.')

#
# Public API Headers
#

libide_lsp_public_headers = [
  'libide-lsp.h',
  'ide-lsp-client.h',
  'ide-lsp-completion-item.h',
  'ide-lsp-completion-provider.h',
  'ide-lsp-completion-results.h',
  'ide-lsp-diagnostic-provider.h',
  'ide-lsp-formatter.h',
  'ide-lsp-highlighter.h',
  'ide-lsp-hover-provider.h',
  'ide-lsp-rename-provider.h',
  'ide-lsp-symbol-node.h',
  'ide-lsp-symbol-resolver.h',
  'ide-lsp-symbol-tree.h',
  'ide-lsp-types.h',
]

libide_lsp_private_headers = [
  'ide-lsp-util.h',
  'ide-lsp-symbol-node-private.h',
  'ide-lsp-symbol-tree-private.h',
]

install_headers(libide_lsp_public_headers, subdir: libide_lsp_header_subdir)

#
# Sources
#

libide_lsp_public_sources = [
  'ide-lsp-client.c',
  'ide-lsp-completion-item.c',
  'ide-lsp-completion-provider.c',
  'ide-lsp-completion-results.c',
  'ide-lsp-diagnostic-provider.c',
  'ide-lsp-formatter.c',
  'ide-lsp-highlighter.c',
  'ide-lsp-hover-provider.c',
  'ide-lsp-rename-provider.c',
  'ide-lsp-symbol-node.c',
  'ide-lsp-symbol-resolver.c',
  'ide-lsp-symbol-tree.c',
]

libide_lsp_private_sources = [
  'ide-lsp-util.c',
]

libide_lsp_sources = libide_lsp_public_sources + libide_lsp_private_sources

#
# Dependencies
#

libide_lsp_deps = [
  libgio_dep,
  libjsonrpc_glib_dep,
  libdazzle_dep,

  libide_code_dep,
  libide_core_dep,
  libide_io_dep,
  libide_projects_dep,
  libide_sourceview_dep,
  libide_threading_dep,
]

#
# Library Definitions
#

libide_lsp = static_library('ide-lsp-' + libide_api_version, libide_lsp_sources,
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
gnome_builder_gir_extra_args += ['--c-include=libide-lsp.h', '-DIDE_LSP_COMPILATION']
