session_headers = [
  'ide-session.h',
  'ide-session-addin.h',
]

session_sources = [
  'ide-session.c',
  'ide-session-addin.c',
]

libide_public_headers += files(session_headers)
libide_public_sources += files(session_sources)

install_headers(session_headers, subdir: join_paths(libide_header_subdir, 'session'))
