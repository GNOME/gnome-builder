completion_headers = [
  'ide-completion-compat.h',
  'ide-completion-context.h',
  'ide-completion-display.h',
  'ide-completion-item.h',
  'ide-completion-list-box-row.h',
  'ide-completion-list-box.h',
  'ide-completion-overlay.h',
  'ide-completion-proposal.h',
  'ide-completion-provider.h',
  'ide-completion-results.h',
  'ide-completion-types.h',
  'ide-completion-view.h',
  'ide-completion-window.h',
  'ide-completion.h',
]

completion_sources = [
  'ide-completion.c',
  'ide-completion-compat.c',
  'ide-completion-context.c',
  'ide-completion-display.c',
  'ide-completion-item.c',
  'ide-completion-list-box.c',
  'ide-completion-list-box-row.c',
  'ide-completion-overlay.c',
  'ide-completion-proposal.c',
  'ide-completion-provider.c',
  'ide-completion-results.c',
  'ide-completion-view.c',
  'ide-completion-window.c',
]

completion_private_sources = [
  'ide-completion-private.h',
# 'ide-completion-words.c',
# 'ide-completion-words.h',
# 'ide-word-completion-item.c',
# 'ide-word-completion-item.h',
# 'ide-word-completion-provider.c',
# 'ide-word-completion-provider.h',
# 'ide-word-completion-results.c',
# 'ide-word-completion-results.h',
]


libide_public_headers += files(completion_headers)
libide_public_sources += files(completion_sources)
libide_private_sources += files(completion_private_sources)

install_headers(completion_headers, subdir: join_paths(libide_header_subdir, 'completion'))
