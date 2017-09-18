workers_headers = [
  'ide-worker.h',
]

workers_sources = [
  'ide-worker.c',
]

workers_private_sources = [
  'ide-worker-manager.c',
  'ide-worker-manager.h',
  'ide-worker-process.c',
  'ide-worker-process.h',
]

libide_public_headers += files(workers_headers)
libide_public_sources += files(workers_sources)
libide_private_sources += files(workers_private_sources)

install_headers(workers_headers, subdir: join_paths(libide_header_subdir, 'workers'))
