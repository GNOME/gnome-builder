snippets_headers = [
  'ide-source-snippet-chunk.h',
  'ide-source-snippet-context.h',
  'ide-source-snippet.h',
  'ide-source-snippets-manager.h',
  'ide-source-snippets.h',
]

snippets_sources = [
  'ide-source-snippet-chunk.c',
  'ide-source-snippet-context.c',
  'ide-source-snippet.c',
  'ide-source-snippets-manager.c',
  'ide-source-snippets.c',
]

snippets_private_sources = [
  'ide-source-snippet-completion-item.c',
  'ide-source-snippet-completion-item.h',
  'ide-source-snippet-completion-provider.c',
  'ide-source-snippet-completion-provider.h',
  'ide-source-snippet-parser.c',
  'ide-source-snippet-parser.h',
  'ide-source-snippet-private.h',
]

libide_public_headers += files(snippets_headers)
libide_public_sources += files(snippets_sources)
libide_private_sources += files(snippets_private_sources)

install_headers(snippets_headers, subdir: join_paths(libide_header_subdir, 'snippets'))
