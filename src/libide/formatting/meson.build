formatting_headers = [
  'ide-formatter.h',
  'ide-formatter-options.h',
]

formatting_sources = [
  'ide-formatter.c',
  'ide-formatter-options.c',
]

libide_public_headers += files(formatting_headers)
libide_public_sources += files(formatting_sources)

install_headers(formatting_headers, subdir: join_paths(libide_header_subdir, 'formatting'))
