transfers_headers = [
  'ide-pkcon-transfer.h',
  'ide-transfer-manager.h',
  'ide-transfer-row.h',
  'ide-transfer.h',
  'ide-transfer-button.h',
  'ide-transfers-button.h',
  'ide-transfers-progress-icon.h',
]

transfers_sources = [
  'ide-pkcon-transfer.c',
  'ide-transfer-manager.c',
  'ide-transfer-row.c',
  'ide-transfer.c',
  'ide-transfer-button.c',
  'ide-transfers-button.c',
  'ide-transfers-progress-icon.c',
]

libide_public_headers += files(transfers_headers)
libide_public_sources += files(transfers_sources)

install_headers(transfers_headers, subdir: join_paths(libide_header_subdir, 'transfers'))
