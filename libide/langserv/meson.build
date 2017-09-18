langserv_headers = [
  'ide-langserv-client.h',
  'ide-langserv-completion-provider.h',
  'ide-langserv-diagnostic-provider.h',
  'ide-langserv-formatter.h',
  'ide-langserv-highlighter.h',
  'ide-langserv-rename-provider.h',
  'ide-langserv-symbol-node.h',
  'ide-langserv-symbol-resolver.h',
  'ide-langserv-symbol-tree.h',
  'ide-langserv-util.h',
]

langserv_sources = [
  'ide-langserv-client.c',
  'ide-langserv-completion-provider.c',
  'ide-langserv-diagnostic-provider.c',
  'ide-langserv-formatter.c',
  'ide-langserv-highlighter.c',
  'ide-langserv-rename-provider.c',
  'ide-langserv-symbol-node.c',
  'ide-langserv-symbol-node-private.h',
  'ide-langserv-symbol-resolver.c',
  'ide-langserv-symbol-tree.c',
  'ide-langserv-symbol-tree-private.h',
  'ide-langserv-util.c',
]

libide_public_headers += files(langserv_headers)
libide_public_sources += files(langserv_sources)

install_headers(langserv_headers, subdir: join_paths(libide_header_subdir, 'langserv'))
