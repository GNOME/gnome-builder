libide_gtk_header_dir = join_paths(libide_header_dir, 'gtk')
libide_gtk_header_subdir = join_paths(libide_header_subdir, 'gtk')
libide_include_directories += include_directories('.')

#
# Public API Headers
#

libide_gtk_public_headers = [
  'ide-animation.h',
  'ide-entry-popover.h',
  'ide-enum-object.h',
  'ide-file-manager.h',
  'ide-font-description.h',
  'ide-gtk.h',
  'ide-install-button.h',
  'ide-joined-menu.h',
  'ide-menu-manager.h',
  'ide-progress-icon.h',
  'ide-radio-box.h',
  'ide-scrubber-revealer.h',
  'ide-search-entry.h',
  'ide-shortcut-accel-dialog.h',
  'ide-tree-expander.h',
  'ide-truncate-model.h',
  'ide-unique-list-model.h',
  'libide-gtk.h',
]

libide_gtk_enum_headers = [
  'ide-animation.h',
  'ide-scrubber-revealer.h',
]

install_headers(libide_gtk_public_headers, subdir: libide_gtk_header_subdir)

#
# Sources
#

libide_gtk_public_sources = [
  'ide-animation.c',
  'ide-entry-popover.c',
  'ide-enum-object.c',
  'ide-file-manager.c',
  'ide-font-description.c',
  'ide-gtk.c',
  'ide-install-button.c',
  'ide-joined-menu.c',
  'ide-menu-manager.c',
  'ide-progress-icon.c',
  'ide-radio-box.c',
  'ide-scrubber-revealer.c',
  'ide-search-entry.c',
  'ide-shortcut-accel-dialog.c',
  'ide-tree-expander.c',
  'ide-truncate-model.c',
  'ide-unique-list-model.c',
]

libide_gtk_private_sources = [
  'ide-frame-source.c',
  'ide-gtk-init.c',
]

#
# Enum generation
#

libide_gtk_enums = gnome.mkenums_simple('ide-gtk-enums',
     body_prefix: '#include "config.h"',
   header_prefix: '#include <libide-core.h>',
       decorator: '_IDE_EXTERN',
         sources: libide_gtk_enum_headers,
  install_header: true,
     install_dir: libide_gtk_header_dir,
)
libide_gtk_generated_sources = [libide_gtk_enums[0]]
libide_gtk_generated_headers = [libide_gtk_enums[1]]

#
# Generated Resource Files
#

libide_gtk_resources = gnome.compile_resources(
  'ide-gtk-resources',
  'libide-gtk.gresource.xml',
  c_name: 'ide_gtk',
)
libide_gtk_generated_sources += [libide_gtk_resources[0]]
libide_gtk_generated_headers += [libide_gtk_resources[1]]

#
# Dependencies
#

libide_gtk_deps = [
  libgio_dep,
  libgtk_dep,
  libadwaita_dep,

  libide_core_dep,
  libide_io_dep,
  libide_threading_dep,
]

#
# Library Definitions
#

libide_gtk = static_library('ide-gtk-' + libide_api_version,
   libide_gtk_public_sources + libide_gtk_private_sources + libide_gtk_generated_sources + libide_gtk_generated_headers,
   dependencies: libide_gtk_deps,
         c_args: libide_args + release_args + ['-DIDE_GTK_COMPILATION'],
)

libide_gtk_dep = declare_dependency(
         dependencies: libide_gtk_deps,
            link_with: libide_gtk,
  include_directories: include_directories('.'),
             sources: libide_gtk_generated_headers,
)

gnome_builder_public_sources += files(libide_gtk_public_sources)
gnome_builder_public_headers += files(libide_gtk_public_headers)
gnome_builder_generated_headers += libide_gtk_generated_headers
gnome_builder_generated_sources += libide_gtk_generated_sources
gnome_builder_include_subdirs += libide_gtk_header_subdir
gnome_builder_gir_extra_args += '-DIDE_GTK_COMPILATION'
gnome_builder_main_headers += 'libide-gtk.h'
