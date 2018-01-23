util_headers = [
  'ide-cell-renderer-fancy.h',
  'ide-fancy-tree-view.h',
  'ide-flatpak.h',
  'ide-glib.h',
  'ide-gtk.h',
  'ide-line-reader.h',
  'ide-list-inline.h',
  'ide-posix.h',
  'ide-progress.h',
  'ide-ref-ptr.h',
  'ide-settings.h',
  'ide-uri.h',
]

util_sources = [
  'ide-cell-renderer-fancy.c',
  'ide-fancy-tree-view.c',
  'ide-flatpak.c',
  'ide-glib.c',
  'ide-gtk.c',
  'ide-line-reader.c',
  'ide-posix.c',
  'ide-progress.c',
  'ide-ref-ptr.c',
  'ide-settings.c',
  'ide-uri.c',
]

util_private_sources = [
  'ide-async-helper.c',
  'ide-battery-monitor.c',
  'ide-doc-seq.c',
  'ide-window-settings.c',
]

libide_public_headers += files(util_headers)
libide_public_sources += files(util_sources)
libide_private_sources += files(util_private_sources)

install_headers(util_headers, subdir: join_paths(libide_header_subdir, 'util'))
