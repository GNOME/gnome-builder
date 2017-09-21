application_headers = [
  'ide-application.h',
  'ide-application-addin.h',
  'ide-application-credits.h',
  'ide-application-tool.h',
]

application_sources = [
  'ide-application.c',
  'ide-application-addin.c',
  'ide-application-open.c',
  'ide-application-tool.c',
]

application_private_sources = [
  'ide-application-actions.c',
  'ide-application-actions.h',
  'ide-application-command-line.c',
  'ide-application-plugins.c',
  'ide-application-private.h',
  'ide-application-shortcuts.c',
  'ide-application-tests.c',
  'ide-application-tests.h',
]

libide_public_headers += files(application_headers)
libide_public_sources += files(application_sources)
libide_private_sources += files(application_private_sources)

install_headers(application_headers, subdir: join_paths(libide_header_subdir, 'application'))
