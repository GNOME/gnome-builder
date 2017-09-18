template_headers = [
  'ide-project-template.h',
  'ide-template-base.h',
  'ide-template-provider.h',
]

template_sources = [
  'ide-project-template.c',
  'ide-template-base.c',
  'ide-template-provider.c',
]

libide_public_headers += files(template_headers)
libide_public_sources += files(template_sources)

install_headers(template_headers, subdir: join_paths(libide_header_subdir, 'template'))
