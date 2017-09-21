layout_headers = [
  'ide-layout-grid.h',
  'ide-layout-grid-column.h',
  'ide-layout-stack.h',
  'ide-layout-stack-addin.h',
  'ide-layout-stack-header.h',
  'ide-layout-view.h',
  'ide-layout-pane.h',
  'ide-layout-transient-sidebar.h',
  'ide-layout.h',
]

layout_sources = [
  'ide-layout-grid.c',
  'ide-layout-grid-column.c',
  'ide-layout-pane.c',
  'ide-layout-stack.c',
  'ide-layout-stack-addin.c',
  'ide-layout-stack-header.c',
  'ide-layout-transient-sidebar.c',
  'ide-layout-view.c',
  'ide-layout.c',
]

layout_private_sources = [
  'ide-layout-grid-actions.c',
  'ide-layout-grid-column-actions.c',
  'ide-layout-private.h',
  'ide-layout-stack-actions.c',
  'ide-layout-stack-shortcuts.c',
  'ide-shortcut-label.c',
  'ide-shortcut-label.h',
]

libide_public_headers += files(layout_headers)
libide_public_sources += files(layout_sources)
libide_private_sources += files(layout_private_sources)

install_headers(layout_headers, subdir: join_paths(libide_header_subdir, 'layout'))
