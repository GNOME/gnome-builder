threading_headers = [
  'ide-thread-pool.h',
  'ide-task.h',
]

threading_sources = [
  'ide-thread-pool.c',
  'ide-task.c',
]

threading_enums = [
  'ide-thread-pool.h',
  'ide-task.h',
]

libide_public_headers += files(threading_headers)
libide_public_sources += files(threading_sources)
libide_enum_headers += files(threading_enums)

install_headers(threading_headers, subdir: join_paths(libide_header_subdir, 'threading'))
