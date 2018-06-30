hover_headers = [
  'ide-hover-context.h',
  'ide-hover-provider.h',
  'ide-marked-content.h',
  'ide-marked-view.h',
]

hover_sources = [
  'ide-hover-context.c',
  'ide-hover-provider.c',
  'ide-marked-content.c',
  'ide-marked-view.c',
]

hover_private_sources = [
  'ide-hover.c',
  'ide-hover-popover.c',
]

hover_enums = [
  'ide-marked-content.h',
]

libide_public_headers += files(hover_headers)
libide_public_sources += files(hover_sources)
libide_private_sources += files(hover_private_sources)
libide_enum_headers += files(hover_enums)

install_headers(hover_headers, subdir: join_paths(libide_header_subdir, 'hover'))
