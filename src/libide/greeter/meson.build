greeter_headers = [
  'ide-greeter-section.h',
]

greeter_sources = [
  'ide-greeter-section.c',
]

greeter_private_sources = [
  'ide-greeter-perspective.c',
  'ide-greeter-perspective.h',
  'ide-greeter-project-row.c',
  'ide-greeter-project-row.h',
]

libide_public_headers += files(greeter_headers)
libide_public_sources += files(greeter_sources)
libide_private_sources += files(greeter_private_sources)

install_headers(greeter_headers, subdir: join_paths(libide_header_subdir, 'greeter'))
