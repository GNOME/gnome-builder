directory_headers = [
  'ide-documentation.h',
  'ide-documentation-info.h',
  'ide-documentation-proposal.h',
  'ide-documentation-provider.h',
]

directory_sources = [
  'ide-documentation.c',
  'ide-documentation-info.c',
  'ide-documentation-proposal.c',
  'ide-documentation-provider.c',
]

libide_public_headers += files(directory_headers)
libide_public_sources += files(directory_sources)

install_headers(directory_headers, subdir: join_paths(libide_header_subdir, 'directory'))
