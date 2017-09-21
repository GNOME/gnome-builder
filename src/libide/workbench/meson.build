workbench_headers = [
  'ide-omni-bar.h',
  'ide-perspective.h',
  'ide-workbench-addin.h',
  'ide-workbench-message.h',
  'ide-workbench-header-bar.h',
  'ide-workbench.h',
]

workbench_sources = [
  'ide-omni-bar.c',
  'ide-perspective.c',
  'ide-workbench-addin.c',
  'ide-workbench-message.c',
  'ide-workbench-header-bar.c',
  'ide-workbench-open.c',
  'ide-workbench.c',
]

workbench_private_sources = [
  'ide-omni-pausable-row.c',
  'ide-omni-pausable-row.h',
  'ide-workbench-actions.c',
  'ide-workbench-private.h',
  'ide-workbench-shortcuts.c',
]

libide_public_headers += files(workbench_headers)
libide_public_sources += files(workbench_sources)
libide_private_sources += files(workbench_private_sources)

install_headers(workbench_headers, subdir: join_paths(libide_header_subdir, 'workbench'))
