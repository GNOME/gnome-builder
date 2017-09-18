search_headers = [
  'ide-search-engine.h',
  'ide-search-entry.h',
  'ide-search-provider.h',
  'ide-search-result.h',
]

search_sources = [
  'ide-search-engine.c',
  'ide-search-entry.c',
  'ide-search-provider.c',
  'ide-search-result.c',
]

search_private_sources = [
  'ide-search-reducer.c',
  'ide-search-reducer.h',
]

libide_public_headers += files(search_headers)
libide_public_sources += files(search_sources)
libide_private_sources += files(search_private_sources)

install_headers(search_headers, subdir: join_paths(libide_header_subdir, 'search'))
