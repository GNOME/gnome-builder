doap_headers = [
  'ide-doap-person.h',
  'ide-doap.h',
]

doap_sources = [
  'ide-doap-person.c',
  'ide-doap.c',
]

doap_enums = [
  'ide-doap.h',
]

libide_public_headers += files(doap_headers)
libide_public_sources += files(doap_sources)
libide_enum_headers += files(doap_enums)

install_headers(doap_headers, subdir: join_paths(libide_header_subdir, 'doap'))
