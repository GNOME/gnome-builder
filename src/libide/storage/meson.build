storage_headers = [
  'ide-persistent-map.h',
  'ide-persistent-map-builder.h',
]

storage_sources = [
  'ide-persistent-map.c',
  'ide-persistent-map-builder.c',
]

libide_public_headers += files(storage_headers)
libide_public_sources += files(storage_sources)

install_headers(storage_headers, subdir: join_paths(libide_header_subdir, 'storage'))
