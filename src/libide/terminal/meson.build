terminal_headers = [
  'ide-terminal.h',
  'ide-terminal-search.h',
  'ide-terminal-util.h',
]

terminal_sources = [
  'ide-terminal.c',
  'ide-terminal-search.c',
  'ide-terminal-util.c',
]

libide_public_headers += files(terminal_headers)
libide_public_sources += files(terminal_sources)

install_headers(terminal_headers, subdir: join_paths(libide_header_subdir, 'terminal'))
