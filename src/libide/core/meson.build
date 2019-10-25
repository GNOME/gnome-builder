libide_core_header_dir = join_paths(libide_header_dir, 'core')
libide_core_header_subdir = join_paths(libide_header_subdir, 'core')
libide_include_directories += include_directories('.')

#
# Versioning that all libide libraries (re)use
#

version_data = configuration_data()
version_data.set('MAJOR_VERSION', MAJOR_VERSION)
version_data.set('MINOR_VERSION', MINOR_VERSION)
version_data.set('MICRO_VERSION', MICRO_VERSION)
version_data.set('VERSION', meson.project_version())
version_data.set_quoted('BUILD_CHANNEL', get_option('channel'))
version_data.set_quoted('BUILD_TYPE', get_option('buildtype'))

libide_core_version_h = configure_file(
          input: 'ide-version.h.in',
         output: 'ide-version.h',
    install_dir: libide_core_header_dir,
        install: true,
  configuration: version_data)

libide_core_generated_headers = [libide_core_version_h]

libide_build_ident_h = vcs_tag(
     fallback: meson.project_version(),
        input: 'ide-build-ident.h.in',
       output: 'ide-build-ident.h',
)
libide_core_generated_headers += [libide_build_ident_h]

#
# Debugging and Tracing Support
#

libide_core_conf = configuration_data()
libide_core_conf.set10('ENABLE_TRACING', get_option('tracing'))
libide_core_conf.set('BUGREPORT_URL', 'https://gitlab.gnome.org/GNOME/gnome-builder/issues')

libide_debug_h = configure_file(
         input: 'ide-debug.h.in',
         output: 'ide-debug.h',
  configuration: libide_core_conf,
        install: true,
    install_dir: libide_core_header_dir,
)

libide_core_generated_headers += [libide_debug_h]

#
# Public API Headers
#

libide_core_public_headers = [
  'ide-context.h',
  'ide-context-addin.h',
  'ide-global.h',
  'ide-log.h',
  'ide-macros.h',
  'ide-notification.h',
  'ide-notifications.h',
  'ide-object.h',
  'ide-object-box.h',
  'ide-settings.h',
  'ide-transfer.h',
  'ide-transfer-manager.h',
  'ide-version-macros.h',
  'libide-core.h',
]

libide_core_private_headers = [
  'ide-transfer-manager-private.h',
]

install_headers(libide_core_public_headers, subdir: libide_core_header_subdir)

#
# Sources
#

libide_core_public_sources = [
  'ide-context.c',
  'ide-context-addin.c',
  'ide-global.c',
  'ide-log.c',
  'ide-notification.c',
  'ide-notifications.c',
  'ide-object.c',
  'ide-object-box.c',
  'ide-object-notify.c',
  'ide-settings.c',
  'ide-transfer.c',
  'ide-transfer-manager.c',
]

libide_core_sources = []
libide_core_sources += libide_core_generated_headers
libide_core_sources += libide_core_public_sources

#
# Library Definitions
#

libide_core_deps = [
  libgio_dep,
  libgtk_dep,
  libdazzle_dep,
  libpeas_dep,
]

libide_core = static_library('ide-core-' + libide_api_version, libide_core_sources,
   dependencies: libide_core_deps,
         c_args: libide_args + release_args + ['-DIDE_CORE_COMPILATION'],
)

libide_core_dep = declare_dependency(
              sources: libide_core_private_headers + libide_core_generated_headers,
         dependencies: libide_core_deps,
            link_with: libide_core,
  include_directories: include_directories('.'),
)

gnome_builder_public_sources += files(libide_core_public_sources)
gnome_builder_public_headers += files(libide_core_public_headers)
gnome_builder_private_headers += files(libide_core_private_headers)
gnome_builder_generated_headers += libide_core_generated_headers
gnome_builder_include_subdirs += libide_core_header_subdir
gnome_builder_gir_extra_args += ['--c-include=libide-core.h', '-DIDE_CORE_COMPILATION']
