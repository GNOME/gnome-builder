preferences_headers = [
  'ide-preferences-addin.h',
  'ide-preferences-perspective.h',
]

preferences_sources = [
  'ide-preferences-addin.c',
  'ide-preferences-perspective.c',
]

preferences_private_sources = [
  'ide-preferences-builtin.c',
  'ide-preferences-builtin.h',
  'ide-preferences-language-row.c',
  'ide-preferences-language-row.h',
]

libide_public_headers += files(preferences_headers)
libide_public_sources += files(preferences_sources)
libide_private_sources += files(preferences_private_sources)

install_headers(preferences_headers, subdir: join_paths(libide_header_subdir, 'preferences'))
