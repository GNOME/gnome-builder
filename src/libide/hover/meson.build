hover_headers = [
  'ide-hover-context.h',
  'ide-hover-provider.h',
]

hover_sources = [
  'ide-hover-context.c',
  'ide-hover-provider.c',
]

hover_private_sources = [
  'ide-hover.c',
  'ide-hover-popover.c',
]

libide_public_headers += files(hover_headers)
libide_public_sources += files(hover_sources)
libide_private_sources += files(hover_private_sources)

install_headers(hover_headers, subdir: join_paths(libide_header_subdir, 'hover'))
