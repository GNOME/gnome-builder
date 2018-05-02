completion_headers = [
  'ide-completion-item.h',
  'ide-completion-provider.h',
  'ide-completion-results.h',
]

completion_sources = [
  'ide-completion-item.c',
  'ide-completion-provider.c',
  'ide-completion-results.c',
]

completion_private_sources = [
  'ide-completion-words.c',
  'ide-completion-words.h',
  'ide-word-completion-item.c',
  'ide-word-completion-item.h',
  'ide-word-completion-provider.c',
  'ide-word-completion-provider.h',
  'ide-word-completion-results.c',
  'ide-word-completion-results.h',
]


libide_public_headers += files(completion_headers)
libide_public_sources += files(completion_sources)
libide_private_sources += files(completion_private_sources)

install_headers(completion_headers, subdir: join_paths(libide_header_subdir, 'completion'))
