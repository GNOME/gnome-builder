directory_headers = [
  'ide-directory-build-system.h',
  'ide-directory-vcs.h',
]

directory_sources = [
  'ide-directory-build-system.c',
  'ide-directory-plugin.c',
  'ide-directory-vcs.c',
]

libide_public_headers += files(directory_headers)
libide_public_sources += files(directory_sources)

install_headers(directory_headers, subdir: join_paths(libide_header_subdir, 'directory'))
