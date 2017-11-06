runtimes_headers = [
  'ide-runtime-manager.h',
  'ide-runtime-provider.h',
  'ide-runtime.h',
]

runtimes_sources = [
  'ide-runtime-manager.c',
  'ide-runtime-provider.c',
  'ide-runtime.c',
]

runtimes_enums = [
  'ide-runtime.h',
]

libide_public_headers += files(runtimes_headers)
libide_public_sources += files(runtimes_sources)
libide_enum_headers += files(runtimes_enums)

install_headers(runtimes_headers, subdir: join_paths(libide_header_subdir, 'runtimes'))
