libide_tweaks_header_dir = join_paths(libide_header_dir, 'tweaks')
libide_tweaks_header_subdir = join_paths(libide_header_subdir, 'tweaks')
libide_include_directories += include_directories('.')

#
# Public API Headers
#

libide_tweaks_public_headers = [
  'libide-tweaks.h',
  'ide-tweaks.h',
  'ide-tweaks-addin.h',
  'ide-tweaks-binding.h',
  'ide-tweaks-caption.h',
  'ide-tweaks-choice.h',
  'ide-tweaks-combo.h',
  'ide-tweaks-combo-row.h',
  'ide-tweaks-directory.h',
  'ide-tweaks-entry.h',
  'ide-tweaks-factory.h',
  'ide-tweaks-font.h',
  'ide-tweaks-group.h',
  'ide-tweaks-info.h',
  'ide-tweaks-item.h',
  'ide-tweaks-page.h',
  'ide-tweaks-password.h',
  'ide-tweaks-property.h',
  'ide-tweaks-radio.h',
  'ide-tweaks-section.h',
  'ide-tweaks-setting.h',
  'ide-tweaks-spin.h',
  'ide-tweaks-switch.h',
  'ide-tweaks-variant.h',
  'ide-tweaks-widget.h',
  'ide-tweaks-window.h',
]

install_headers(libide_tweaks_public_headers, subdir: libide_tweaks_header_subdir)

#
# Sources
#

libide_tweaks_public_sources = [
  'ide-tweaks.c',
  'ide-tweaks-addin.c',
  'ide-tweaks-binding.c',
  'ide-tweaks-caption.c',
  'ide-tweaks-choice.c',
  'ide-tweaks-combo.c',
  'ide-tweaks-combo-row.c',
  'ide-tweaks-directory.c',
  'ide-tweaks-entry.c',
  'ide-tweaks-factory.c',
  'ide-tweaks-font.c',
  'ide-tweaks-group.c',
  'ide-tweaks-info.c',
  'ide-tweaks-item.c',
  'ide-tweaks-page.c',
  'ide-tweaks-password.c',
  'ide-tweaks-property.c',
  'ide-tweaks-radio.c',
  'ide-tweaks-section.c',
  'ide-tweaks-setting.c',
  'ide-tweaks-spin.c',
  'ide-tweaks-switch.c',
  'ide-tweaks-variant.c',
  'ide-tweaks-widget.c',
  'ide-tweaks-window.c',
]

libide_tweaks_private_sources = [
  'ide-tweaks-init.c',
  'ide-tweaks-model.c',
  'ide-tweaks-panel.c',
  'ide-tweaks-panel-list.c',
  'ide-tweaks-panel-list-row.c',
]

#
# Generated Resource Files
#

libide_tweaks_resources = gnome.compile_resources(
  'ide-tweaks-resources',
  'libide-tweaks.gresource.xml',
  c_name: 'ide_tweaks',
)

#
# Dependencies
#

libide_tweaks_deps = [
  libgio_dep,
  libgtk_dep,
  libadwaita_dep,

  libide_core_dep,
  libide_io_dep,
  libide_search_dep,
]

#
# Library Definitions
#

libide_tweaks = static_library('ide-tweaks-' + libide_api_version,
   libide_tweaks_public_sources + libide_tweaks_private_sources + libide_tweaks_resources,
   dependencies: libide_tweaks_deps,
         c_args: libide_args + release_args + ['-DIDE_TWEAKS_COMPILATION'],
)

libide_tweaks_dep = declare_dependency(
         dependencies: libide_tweaks_deps,
            link_with: libide_tweaks,
  include_directories: include_directories('.'),
)

gnome_builder_public_sources += files(libide_tweaks_public_sources)
gnome_builder_public_headers += files(libide_tweaks_public_headers)
gnome_builder_include_subdirs += libide_tweaks_header_subdir
gnome_builder_gir_extra_args += '-DIDE_TWEAKS_COMPILATION'
gnome_builder_main_headers += 'libide-tweaks.h'

subdir('tests')
