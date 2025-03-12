if libwebkit_dep.found()

libide_webkit_header_dir = join_paths(libide_header_dir, 'webkit')
libide_webkit_header_subdir = join_paths(libide_header_subdir, 'webkit')
libide_include_directories += include_directories('.')

#
# Sources
#

libide_webkit_private_sources = [
  'ide-text-buffer-html-generator.c',
  'ide-url-bar.c',
  'ide-webkit-plugin.c',
  'ide-webkit-util.c',
  'ide-webkit-workspace.c',
]

#
# Public API Headers
#

libide_webkit_public_headers = [
  'libide-webkit.h',
  'ide-html-generator.h',
  'ide-webkit-page.h',
]

libide_webkit_public_sources = [
  'ide-html-generator.c',
  'ide-webkit-page.c',
]

#
# Generated Resource Files
#

libide_webkit_resources = gnome.compile_resources(
  'ide-webkit-resources',
  'libide-webkit.gresource.xml',
  c_name: 'ide_webkit',
)
libide_webkit_generated_headers = [libide_webkit_resources[1]]

libide_webkit_sources = libide_webkit_resources + libide_webkit_public_sources + libide_webkit_private_sources

#
# Dependencies
#

libide_webkit_deps = [
  libwebkit_dep,
  libpeas_dep,
  libide_gui_dep,
]

gnome_builder_pkg_requires += [webkit_api]
gnome_builder_gir_includes += [webkit_gir]

#
# Library Definitions
#

libide_webkit = static_library('ide-webkit-' + libide_api_version, libide_webkit_sources,
         dependencies: libide_webkit_deps,
               c_args: libide_args + release_args + ['-DIDE_WEBKIT_COMPILATION'],
  include_directories: include_directories('..'),
)

libide_webkit_dep = declare_dependency(
         dependencies: libide_webkit_deps,
            link_with: libide_webkit,
  include_directories: [include_directories('.'), include_directories('..')],
              sources: libide_webkit_generated_headers,
)

gnome_builder_private_sources += files(libide_webkit_private_sources)
gnome_builder_public_sources += files(libide_webkit_public_sources)
gnome_builder_public_headers += files(libide_webkit_public_headers)
gnome_builder_include_subdirs += libide_webkit_header_subdir
gnome_builder_gir_extra_args += '-DIDE_WEBKIT_COMPILATION'
gnome_builder_main_headers += 'libide-webkit.h'

install_headers(libide_webkit_public_headers, subdir: libide_webkit_header_subdir)

endif
