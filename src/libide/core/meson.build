libide_core_header_dir = join_paths(libide_header_dir, 'core')
libide_core_header_subdir = join_paths(libide_header_subdir, 'core')
libide_include_directories += include_directories('.')

#
# Versioning that all libide libraries (re)use
#

version_data = configuration_data()
version_data.set('MAJOR_VERSION', MAJOR_VERSION)
version_data.set('MINOR_VERSION', MINOR_VERSION)
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
      command: ['git', 'describe'],
     fallback: meson.project_version(),
        input: 'ide-build-ident.h.in',
       output: 'ide-build-ident.h',
)
libide_core_generated_headers += [libide_build_ident_h]

#
# Debugging and Tracing Support
#

libide_core_conf = configuration_data()
libide_core_conf.set10('ENABLE_TRACING', get_option('development'))
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
# Marshallers for use by plugins
#

ide_marshal = gnome.genmarshal('ide-marshal',
            internal: true,
              prefix: 'ide_marshal',
             sources: ['ide-marshal.defs'],
  valist_marshallers: true,
)

#
# Public API Headers
#

libide_core_public_headers = [
  'ide-action-group.h',
  'ide-action-muxer.h',
  'ide-context.h',
  'ide-global.h',
  'ide-gsettings-action-group.h',
  'ide-log.h',
  'ide-log-item.h',
  'ide-macros.h',
  'ide-notification.h',
  'ide-notifications.h',
  'ide-object-box.h',
  'ide-object.h',
  'ide-property-action-group.h',
  'ide-settings-flag-action.h',
  'ide-settings.h',
  'ide-transfer-manager.h',
  'ide-transfer.h',
  'ide-version-macros.h',
  'libide-core.h',
]

libide_core_private_headers = [
  'gsettings-mapping.h',
  'ide-layered-settings-private.h',
  'ide-log-item-private.h',
  'ide-log-model-private.h',
  'ide-transfer-manager-private.h',
]

install_headers(libide_core_public_headers, subdir: libide_core_header_subdir)

#
# Sources
#

libide_core_public_sources = [
  'ide-action-muxer.c',
  'ide-context.c',
  'ide-global.c',
  'ide-gsettings-action-group.c',
  'ide-log.c',
  'ide-log-item.c',
  'ide-notification.c',
  'ide-notifications.c',
  'ide-object-box.c',
  'ide-object-notify.c',
  'ide-object.c',
  'ide-property-action-group.c',
  'ide-settings-flag-action.c',
  'ide-settings.c',
  'ide-transfer-manager.c',
  'ide-transfer.c',
]

libide_core_private_sources = [
  'gsettings-mapping.c',
  'ide-layered-settings.c',
  'ide-log-model.c',
]

libide_core_sources = []
libide_core_sources += libide_core_generated_headers
libide_core_sources += libide_core_public_sources
libide_core_sources += libide_core_private_sources
libide_core_sources += ide_marshal

#
# Library Definitions
#

libide_core_deps = [
  libdex_dep,
  libgio_dep,
  libgir_dep,
  libgtk_dep,
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
gnome_builder_gir_extra_args += '-DIDE_CORE_COMPILATION'
gnome_builder_main_headers += 'libide-core.h'
