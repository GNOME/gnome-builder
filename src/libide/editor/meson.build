editor_headers = [
  'ide-editor-addin.h',
  'ide-editor-perspective.h',
  'ide-editor-search.h',
  'ide-editor-sidebar.h',
  'ide-editor-utilities.h',
  'ide-editor-view-addin.h',
  'ide-editor-view.h',
]

editor_sources = [
  'ide-editor-addin.c',
  'ide-editor-perspective.c',
  'ide-editor-search.c',
  'ide-editor-sidebar.c',
  'ide-editor-utilities.c',
  'ide-editor-view-addin.c',
  'ide-editor-view.c',
]

# .h files used for gtk-doc ignores
editor_private_sources = [
  'ide-editor-hover-provider.c',
  'ide-editor-hover-provider.h',
  'ide-editor-layout-stack-addin.c',
  'ide-editor-layout-stack-addin.h',
  'ide-editor-layout-stack-controls.c',
  'ide-editor-layout-stack-controls.h',
  'ide-editor-perspective-actions.c',
  'ide-editor-perspective-shortcuts.c',
  'ide-editor-plugin.c',
  'ide-editor-print-operation.c',
  'ide-editor-print-operation.h',
  'ide-editor-properties.c',
  'ide-editor-properties.h',
  'ide-editor-search-bar.c',
  'ide-editor-search-bar.h',
  'ide-editor-search-bar-shortcuts.c',
  'ide-editor-session-addin.c',
  'ide-editor-session-addin.h',
  'ide-editor-view-actions.c',
  'ide-editor-view-settings.c',
  'ide-editor-view-shortcuts.c',
  'ide-editor-workbench-addin.c',
  'ide-editor-workbench-addin.h',
]

libide_public_headers += files(editor_headers)
libide_public_sources += files(editor_sources)
libide_private_sources += files(editor_private_sources)

install_headers(editor_headers, subdir: join_paths(libide_header_subdir, 'editor'))
