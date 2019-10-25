
#
# Sources
#

libide_webkit_sources = [
  'ide-webkit-plugin.c',
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
libide_webkit_sources += libide_webkit_resources

#
# Dependencies
#

libide_webkit_deps = [
  libwebkit_dep,
  libpeas_dep,
]

#
# Library Definitions
#

libide_webkit = static_library('ide-webkit-' + libide_api_version, libide_webkit_sources,
   dependencies: libide_webkit_deps,
         c_args: libide_args + release_args + ['-DIDE_WEBKIT_COMPILATION'],
)

libide_webkit_dep = declare_dependency(
         dependencies: libide_webkit_deps,
            link_with: libide_webkit,
  include_directories: include_directories('.'),
              sources: libide_webkit_generated_headers,
)
