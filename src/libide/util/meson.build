util_headers = [
  'ide-cell-renderer-fancy.h',
  'ide-fancy-tree-view.h',
  'ide-flatpak.h',
  'ide-glib.h',
  'ide-gtk.h',
  'ide-line-reader.h',
  'ide-list-inline.h',
  'ide-marked-content.h',
  'ide-marked-view.h',
  'ide-posix.h',
  'ide-progress.h',
  'ide-ref-ptr.h',
  'ide-settings.h',
  'ide-triplet.h',
  'ide-uri.h',
]

util_sources = [
  'ide-cell-renderer-fancy.c',
  'ide-fancy-tree-view.c',
  'ide-flatpak.c',
  'ide-glib.c',
  'ide-gtk.c',
  'ide-line-reader.c',
  'ide-marked-content.c',
  'ide-marked-view.c',
  'ide-posix.c',
  'ide-progress.c',
  'ide-ref-ptr.c',
  'ide-settings.c',
  'ide-triplet.c',
  'ide-uri.c',
]

# .h files used for gtk-doc ignores
util_private_sources = [
  'gs-markdown.c',
  'gs-markdown.h',
  'ide-async-helper.c',
  'ide-async-helper.h',
  'ide-battery-monitor.c',
  'ide-battery-monitor.h',
  'ide-doc-seq.c',
  'ide-doc-seq.h',
  'ide-window-settings.c',
  'ide-window-settings.h',
  'ptyintercept.c',
  'ptyintercept.h',
]

util_enums = [
  'ide-marked-content.h',
]

libide_public_headers += files(util_headers)
libide_public_sources += files(util_sources)
libide_private_sources += files(util_private_sources)
libide_enum_headers += files(util_enums)

install_headers(util_headers, subdir: join_paths(libide_header_subdir, 'util'))
