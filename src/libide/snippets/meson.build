snippets_headers = [
  'ide-snippet-chunk.h',
  'ide-snippet-context.h',
  'ide-snippet-storage.h',
  'ide-snippet-parser.h',
  'ide-snippet.h',
]

snippets_sources = [
  'ide-snippet.c',
  'ide-snippet-chunk.c',
  'ide-snippet-context.c',
  'ide-snippet-parser.c',
  'ide-snippet-storage.c',
]

snippets_private_sources = [
  'ide-snippet-private.h',
]

libide_public_headers += files(snippets_headers)
libide_public_sources += files(snippets_sources)
libide_private_sources += files(snippets_private_sources)

install_headers(snippets_headers, subdir: join_paths(libide_header_subdir, 'snippets'))
