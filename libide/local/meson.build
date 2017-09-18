local_headers = [
  'ide-local-device.h',
]

local_sources = [
  'ide-local-device.c',
]

libide_public_headers += files(local_headers)
libide_public_sources += files(local_sources)

install_headers(local_headers, subdir: join_paths(libide_header_subdir, 'local'))
