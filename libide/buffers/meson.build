buffers_headers = [
  'ide-buffer.h',
  'ide-buffer-addin.h',
  'ide-buffer-change-monitor.h',
  'ide-buffer-manager.h',
  'ide-unsaved-file.h',
  'ide-unsaved-files.h',
]

buffers_sources = [
  'ide-buffer.c',
  'ide-buffer-addin.c',
  'ide-buffer-change-monitor.c',
  'ide-buffer-manager.c',
  'ide-unsaved-file.c',
  'ide-unsaved-files.c',
]

buffers_private_sources = [
  'ide-buffer-private.h',
]

buffers_enums = [
  'ide-buffer.h',
]

libide_enum_headers += files(buffers_enums)
libide_public_headers += files(buffers_headers)
libide_public_sources += files(buffers_sources)
libide_private_sources += files(buffers_private_sources)

install_headers(buffers_headers, subdir: join_paths(libide_header_subdir, 'buffers'))
