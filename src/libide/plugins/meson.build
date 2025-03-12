libide_plugins_header_subdir = join_paths(libide_header_subdir, 'plugins')
libide_include_directories += include_directories('.')

#
# Public API Headers
#

libide_plugins_public_headers = [
  'ide-extension-adapter.h',
  'ide-extension-set-adapter.h',
  'ide-plugin.h',
  'ide-plugin-section.h',
  'libide-plugins.h',
]

libide_plugins_private_headers = [
  'ide-extension-util-private.h',
  'ide-plugin-private.h',
  'ide-plugin-section-private.h',
  'ide-plugin-view.h',
]

install_headers(libide_plugins_public_headers, subdir: libide_plugins_header_subdir)

#
# Sources
#

libide_plugins_public_sources = [
  'ide-extension-adapter.c',
  'ide-extension-set-adapter.c',
  'ide-plugin.c',
  'ide-plugin-section.c',
]

libide_plugins_private_sources = [
  'ide-extension-util.c',
  'ide-plugin-view.c',
]

#
# Generated Resource Files
#

libide_plugins_resources = gnome.compile_resources(
  'ide-plugins-resources',
  'libide-plugins.gresource.xml',
  c_name: 'ide_plugins',
)

#
# Library Definitions
#

libide_plugins_deps = [
  libgio_dep,
  libpeas_dep,
  libgtk_dep,

  libide_core_dep,
]

libide_plugins = static_library('ide-plugins-' + libide_api_version,
  libide_plugins_public_sources + libide_plugins_private_sources + libide_plugins_resources,
   dependencies: libide_plugins_deps,
         c_args: libide_args + release_args + ['-DIDE_PLUGINS_COMPILATION'],
)

libide_plugins_dep = declare_dependency(
              sources: libide_plugins_private_headers,
         dependencies: libide_plugins_deps,
            link_with: libide_plugins,
  include_directories: include_directories('.'),
)

gnome_builder_public_sources += files(libide_plugins_public_sources)
gnome_builder_public_headers += files(libide_plugins_public_headers)
gnome_builder_include_subdirs += libide_plugins_header_subdir
gnome_builder_gir_extra_args += '-DIDE_PLUGINS_COMPILATION'
gnome_builder_main_headers += 'libide-plugins.h'
