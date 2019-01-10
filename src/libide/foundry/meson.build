libide_foundry_header_dir = join_paths(libide_header_dir, 'foundry')
libide_foundry_header_subdir = join_paths(libide_header_subdir, 'foundry')
libide_include_directories += include_directories('.')

libide_foundry_sources = []
libide_foundry_public_headers = []
libide_foundry_generated_headers = []

#
# Public API Headers
#

libide_foundry_public_headers = [
  'ide-build-log.h',
  'ide-build-manager.h',
  'ide-build-pipeline-addin.h',
  'ide-build-pipeline.h',
  'ide-build-stage-launcher.h',
  'ide-build-stage-mkdirs.h',
  'ide-build-stage-transfer.h',
  'ide-build-stage.h',
  'ide-build-system-discovery.h',
  'ide-build-system.h',
  'ide-build-target-provider.h',
  'ide-build-target.h',
  'ide-compile-commands.h',
  'ide-configuration-manager.h',
  'ide-configuration-provider.h',
  'ide-configuration.h',
  'ide-dependency-updater.h',
  'ide-deploy-strategy.h',
  'ide-device-info.h',
  'ide-device-manager.h',
  'ide-device-provider.h',
  'ide-device.h',
  'ide-fallback-build-system.h',
  'ide-foundry-types.h',
  'ide-local-device.h',
  'ide-run-manager.h',
  'ide-runner-addin.h',
  'ide-runner.h',
  'ide-runtime-manager.h',
  'ide-runtime-provider.h',
  'ide-runtime.h',
  'ide-simple-build-system-discovery.h',
  'ide-simple-build-target.h',
  'ide-simple-toolchain.h',
  'ide-toolchain-manager.h',
  'ide-toolchain-provider.h',
  'ide-toolchain.h',
  'ide-triplet.h',
  'libide-foundry.h',
]

libide_foundry_private_headers = [
  'ide-build-log-private.h',
  'ide-build-private.h',
  'ide-build-stage-private.h',
  'ide-configuration-private.h',
  'ide-device-private.h',
  'ide-foundry-init.h',
  'ide-run-manager-private.h',
  'ide-runtime-private.h',
  'ide-toolchain-private.h',
]

libide_foundry_enum_headers = [
  'ide-build-log.h',
  'ide-build-pipeline.h',
  'ide-configuration.h',
  'ide-device.h',
  'ide-device-info.h',
  'ide-runtime.h',
  'ide-test.h',
]

install_headers(libide_foundry_public_headers, subdir: libide_foundry_header_subdir)

#
# Sources
#

libide_foundry_public_sources = [
  'ide-build-manager.c',
  'ide-build-pipeline-addin.c',
  'ide-build-pipeline.c',
  'ide-build-stage-launcher.c',
  'ide-build-stage-mkdirs.c',
  'ide-build-stage-transfer.c',
  'ide-build-stage.c',
  'ide-build-system-discovery.c',
  'ide-build-system.c',
  'ide-build-target-provider.c',
  'ide-build-target.c',
  'ide-compile-commands.c',
  'ide-configuration-manager.c',
  'ide-configuration-provider.c',
  'ide-configuration.c',
  'ide-dependency-updater.c',
  'ide-deploy-strategy.c',
  'ide-device-info.c',
  'ide-device-manager.c',
  'ide-device-provider.c',
  'ide-device.c',
  'ide-fallback-build-system.c',
  'ide-foundry-compat.c',
  'ide-local-device.c',
  'ide-run-manager.c',
  'ide-runner-addin.c',
  'ide-runner.c',
  'ide-runtime-manager.c',
  'ide-runtime-provider.c',
  'ide-runtime.c',
  'ide-simple-build-system-discovery.c',
  'ide-simple-build-target.c',
  'ide-simple-toolchain.c',
  'ide-test.c',
  'ide-test-manager.c',
  'ide-test-provider.c',
  'ide-toolchain-manager.c',
  'ide-toolchain-provider.c',
  'ide-toolchain.c',
  'ide-triplet.c',
]


libide_foundry_private_sources = [
  'ide-build-log.c',
  'ide-build-utils.c',
  'ide-foundry-init.c',
]

libide_foundry_sources += libide_foundry_public_sources
libide_foundry_sources += libide_foundry_private_sources

#
# Enum generation
#

libide_foundry_enums = gnome.mkenums_simple('ide-foundry-enums',
     body_prefix: '#include "config.h"',
   header_prefix: '#include <libide-core.h>',
       decorator: '_IDE_EXTERN',
         sources: libide_foundry_enum_headers,
  install_header: true,
     install_dir: libide_foundry_header_dir,
)
libide_foundry_sources += [libide_foundry_enums[0]]
libide_foundry_generated_headers += [libide_foundry_enums[1]]

#
# Dependencies
#

libide_foundry_deps = [
  libgio_dep,
  libgtk_dep,
  libdazzle_dep,
  libpeas_dep,
  libvte_dep,
  libjson_glib_dep,

  libide_core_dep,
  libide_io_dep,
  libide_projects_dep,
  libide_threading_dep,
]

#
# Library Definitions
#

libide_foundry = static_library('ide-foundry-' + libide_api_version,
  libide_foundry_sources, libide_foundry_generated_headers,
   dependencies: libide_foundry_deps,
         c_args: libide_args + release_args + ['-DIDE_FOUNDRY_COMPILATION'],
)

libide_foundry_dep = declare_dependency(
         dependencies: libide_foundry_deps,
           link_whole: libide_foundry,
  include_directories: include_directories('.'),
              sources: libide_foundry_generated_headers,
)

gnome_builder_public_sources += files(libide_foundry_public_sources)
gnome_builder_public_headers += files(libide_foundry_public_headers)
gnome_builder_private_sources += files(libide_foundry_private_sources)
gnome_builder_private_headers += files(libide_foundry_private_headers)
gnome_builder_generated_headers += libide_foundry_generated_headers
gnome_builder_include_subdirs += libide_foundry_header_subdir
gnome_builder_gir_extra_args += ['--c-include=libide-foundry.h', '-DIDE_FOUNDRY_COMPILATION']
