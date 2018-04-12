toolchain_headers = [
  'ide-simple-toolchain.h',
  'ide-toolchain.h',
  'ide-toolchain-provider.h',
  'ide-toolchain-manager.h'
]

toolchain_sources = [
  'ide-simple-toolchain.c',
  'ide-toolchain.c',
  'ide-toolchain-provider.c',
  'ide-toolchain-manager.c'
]

libide_public_headers += files(toolchain_headers)
libide_public_sources += files(toolchain_sources)

install_headers(toolchain_headers, subdir: join_paths(libide_header_subdir, 'toolchain'))
