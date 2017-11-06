highlighting_headers = [
  'ide-highlight-engine.h',
  'ide-highlight-index.h',
  'ide-highlighter.h',
]

highlighting_sources = [
  'ide-highlight-engine.c',
  'ide-highlight-index.c',
  'ide-highlighter.c',
]

highlighting_enums = [
  'ide-highlighter.h',
]

libide_public_headers += files(highlighting_headers)
libide_public_sources += files(highlighting_sources)
libide_enum_headers += files(highlighting_enums)

install_headers(highlighting_headers, subdir: join_paths(libide_header_subdir, 'highlighting'))
