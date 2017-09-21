symbols_headers = [
  'ide-code-index-entries.h',
  'ide-code-index-entry.h',
  'ide-code-indexer.h',
  'ide-symbol-node.h',
  'ide-symbol-resolver.h',
  'ide-symbol-tree.h',
  'ide-symbol.h',
  'ide-tags-builder.h',
]

symbols_sources = [
  'ide-code-index-entries.c',
  'ide-code-index-entry.c',
  'ide-code-indexer.c',
  'ide-symbol-node.c',
  'ide-symbol-resolver.c',
  'ide-symbol-tree.c',
  'ide-symbol.c',
  'ide-tags-builder.c',
]

symbols_enums = [
  'ide-symbol.h',
]

libide_public_headers += files(symbols_headers)
libide_public_sources += files(symbols_sources)
libide_enum_headers += files(symbols_enums)

install_headers(symbols_headers, subdir: join_paths(libide_header_subdir, 'symbols'))
