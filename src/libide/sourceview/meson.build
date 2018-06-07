sourceview_headers = [
  'ide-indenter.h',
  'ide-language.h',
  'ide-source-iter.h',
  'ide-source-map.h',
  'ide-source-style-scheme.h',
  'ide-source-view.h',
  'ide-text-iter.h',
  'ide-source-search-context.h',
]

sourceview_sources = [
  'ide-indenter.c',
  'ide-language.c',
  'ide-source-iter.c',
  'ide-source-map.c',
  'ide-source-style-scheme.c',
  'ide-source-view.c',
  'ide-text-iter.c',
  'ide-source-search-context.c',
]

sourceview_private_sources = [
  'ide-cursor.c',
  'ide-cursor.h',
  'ide-omni-gutter-renderer.c',
  'ide-omni-gutter-renderer.h',
  'ide-line-change-gutter-renderer.c',
  'ide-line-change-gutter-renderer.h',
  'ide-source-view-capture.c',
  'ide-source-view-capture.h',
  'ide-source-view-mode.c',
  'ide-source-view-mode.h',
  'ide-source-view-movements.c',
  'ide-source-view-shortcuts.c',
  'ide-text-util.c',
]

sourceview_enums = [
  'ide-source-view.h',
]

libide_public_headers += files(sourceview_headers)
libide_public_sources += files(sourceview_sources)
libide_private_sources += files(sourceview_private_sources)
libide_enum_headers += files(sourceview_enums)

install_headers(sourceview_headers, subdir: join_paths(libide_header_subdir, 'sourceview'))
