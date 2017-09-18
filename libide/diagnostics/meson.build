diagnostics_headers = [
  'ide-diagnostic.h',
  'ide-diagnostic-provider.h',
  'ide-diagnostics.h',
  'ide-diagnostics-manager.h',
  'ide-fixit.h',
  'ide-source-location.h',
  'ide-source-range.h',
]

diagnostics_sources = [
  'ide-diagnostic.c',
  'ide-diagnostic-provider.c',
  'ide-diagnostics.c',
  'ide-diagnostics-manager.c',
  'ide-fixit.c',
  'ide-source-location.c',
  'ide-source-range.c',
]

diagnostics_enums = [
  'ide-diagnostic.h',
]

libide_enum_headers += files(diagnostics_enums)
libide_public_headers += files(diagnostics_headers)
libide_public_sources += files(diagnostics_sources)

install_headers(diagnostics_headers, subdir: join_paths(libide_header_subdir, 'diagnostics'))
