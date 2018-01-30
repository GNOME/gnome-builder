files_headers = [
  'ide-file.h',
  'ide-file-settings.h',
  'ide-file-settings.defs',
  'ide-indent-style.h',
  'ide-spaces-style.h',
]

files_sources = [
  'ide-file-settings.c',
  'ide-file.c',
]

files_enums = [
  'ide-indent-style.h',
  'ide-spaces-style.h',
]

libide_public_headers += files(files_headers)
libide_public_sources += files(files_sources)
libide_enum_headers += files(files_enums)

install_headers(files_headers, subdir: join_paths(libide_header_subdir, 'files'))
