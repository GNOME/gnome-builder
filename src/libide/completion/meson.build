completion_headers = [
  'ide-completion-context.h',
  'ide-completion-display.h',
  'ide-completion-list-box-row.h',
  'ide-completion-list-box.h',
  'ide-completion-overlay.h',
  'ide-completion-proposal.h',
  'ide-completion-provider.h',
  'ide-completion-types.h',
  'ide-completion-view.h',
  'ide-completion-window.h',
  'ide-completion.h',
]

completion_sources = [
  'ide-completion.c',
  'ide-completion-context.c',
  'ide-completion-display.c',
  'ide-completion-list-box.c',
  'ide-completion-list-box-row.c',
  'ide-completion-overlay.c',
  'ide-completion-proposal.c',
  'ide-completion-provider.c',
  'ide-completion-view.c',
  'ide-completion-window.c',
]

completion_enums = [
  'ide-completion-types.h',
]

completion_private_sources = [
  'ide-completion-private.h',
]


libide_public_headers += files(completion_headers)
libide_public_sources += files(completion_sources)
libide_private_sources += files(completion_private_sources)
libide_enum_headers += files(completion_enums)

install_headers(completion_headers, subdir: join_paths(libide_header_subdir, 'completion'))
