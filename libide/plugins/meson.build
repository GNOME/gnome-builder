plugins_headers = [
  'ide-extension-adapter.h',
  'ide-extension-set-adapter.h',
]

plugins_sources = [
  'ide-extension-adapter.c',
  'ide-extension-set-adapter.c',
]

plugins_private_sources = [
  'ide-extension-util.c',
  'ide-extension-util.h',
]

libide_public_headers += files(plugins_headers)
libide_public_sources += files(plugins_sources)
libide_private_sources += files(plugins_private_sources)

install_headers(plugins_headers, subdir: join_paths(libide_header_subdir, 'plugins'))
