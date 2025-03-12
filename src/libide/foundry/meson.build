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
  'ide-build-system-discovery.h',
  'ide-build-system.h',
  'ide-build-target-provider.h',
  'ide-build-target.h',
  'ide-compile-commands.h',
  'ide-config-manager.h',
  'ide-config-provider.h',
  'ide-config.h',
  'ide-dependency-updater.h',
  'ide-deploy-strategy.h',
  'ide-device-info.h',
  'ide-device-manager.h',
  'ide-device-provider.h',
  'ide-device.h',
  'ide-diagnostic-tool.h',
  'ide-fallback-build-system.h',
  'ide-foundry-compat.h',
  'ide-foundry-global.h',
  'ide-foundry-types.h',
  'ide-local-device.h',
  'ide-path-cache.h',
  'ide-pipeline-addin.h',
  'ide-pipeline-phase.h',
  'ide-pipeline-stage-command.h',
  'ide-pipeline-stage-launcher.h',
  'ide-pipeline-stage-mkdirs.h',
  'ide-pipeline-stage-transfer.h',
  'ide-pipeline-stage.h',
  'ide-pipeline.h',
  'ide-pty.h',
  'ide-run-command.h',
  'ide-run-command-provider.h',
  'ide-run-commands.h',
  'ide-run-context.h',
  'ide-run-manager.h',
  'ide-run-tool.h',
  'ide-runtime-manager.h',
  'ide-runtime-provider.h',
  'ide-runtime.h',
  'ide-sdk.h',
  'ide-sdk-manager.h',
  'ide-sdk-provider.h',
  'ide-simple-build-system-discovery.h',
  'ide-simple-build-target.h',
  'ide-simple-toolchain.h',
  'ide-test.h',
  'ide-test-manager.h',
  'ide-toolchain-manager.h',
  'ide-toolchain-provider.h',
  'ide-toolchain.h',
  'ide-triplet.h',
  'libide-foundry.h',
]

libide_foundry_private_headers = [
  'ide-build-log-private.h',
  'ide-build-private.h',
  'ide-pipeline-stage-private.h',
  'ide-config-private.h',
  'ide-device-private.h',
  'ide-foundry-init.h',
  'ide-local-deploy-strategy.h',
  'ide-no-tool-private.h',
  'ide-run-manager-private.h',
  'ide-run-tool-private.h',
  'ide-runtime-private.h',
  'ide-toolchain-private.h',
]

libide_foundry_enum_headers = [
  'ide-build-log.h',
  'ide-config.h',
  'ide-device.h',
  'ide-device-info.h',
  'ide-pipeline-phase.h',
  'ide-run-command.h',
  'ide-run-context.h',
  'ide-runtime.h',
  'ide-test.h',
]

install_headers(libide_foundry_public_headers, subdir: libide_foundry_header_subdir)

#
# Sources
#

libide_foundry_public_sources = [
  'ide-build-manager.c',
  'ide-build-system-discovery.c',
  'ide-build-system.c',
  'ide-build-target-provider.c',
  'ide-build-target.c',
  'ide-compile-commands.c',
  'ide-config-manager.c',
  'ide-config-provider.c',
  'ide-config.c',
  'ide-dependency-updater.c',
  'ide-deploy-strategy.c',
  'ide-device-info.c',
  'ide-device-manager.c',
  'ide-device-provider.c',
  'ide-device.c',
  'ide-diagnostic-tool.c',
  'ide-fallback-build-system.c',
  'ide-foundry-compat.c',
  'ide-foundry-global.c',
  'ide-local-device.c',
  'ide-path-cache.c',
  'ide-pipeline-addin.c',
  'ide-pipeline-stage-command.c',
  'ide-pipeline-stage-launcher.c',
  'ide-pipeline-stage-mkdirs.c',
  'ide-pipeline-stage-transfer.c',
  'ide-pipeline-stage.c',
  'ide-pipeline.c',
  'ide-pty.c',
  'ide-run-command.c',
  'ide-run-command-provider.c',
  'ide-run-commands.c',
  'ide-run-context.c',
  'ide-run-manager.c',
  'ide-run-tool.c',
  'ide-runtime-manager.c',
  'ide-runtime-provider.c',
  'ide-runtime.c',
  'ide-sdk.c',
  'ide-sdk-manager.c',
  'ide-sdk-provider.c',
  'ide-simple-build-system-discovery.c',
  'ide-simple-build-target.c',
  'ide-simple-toolchain.c',
  'ide-test-manager.c',
  'ide-test.c',
  'ide-toolchain-manager.c',
  'ide-toolchain-provider.c',
  'ide-toolchain.c',
  'ide-triplet.c',
]


libide_foundry_private_sources = [
  'ide-build-log.c',
  'ide-build-utils.c',
  'ide-foundry-init.c',
  'ide-local-deploy-strategy.c',
  'ide-no-tool.c',
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
            link_with: libide_foundry,
  include_directories: include_directories('.'),
              sources: libide_foundry_generated_headers,
)

gnome_builder_public_sources += files(libide_foundry_public_sources)
gnome_builder_public_headers += files(libide_foundry_public_headers)
gnome_builder_private_sources += files(libide_foundry_private_sources)
gnome_builder_private_headers += files(libide_foundry_private_headers)
gnome_builder_generated_headers += libide_foundry_generated_headers
gnome_builder_include_subdirs += libide_foundry_header_subdir
gnome_builder_gir_extra_args += '-DIDE_FOUNDRY_COMPILATION'
gnome_builder_main_headers += 'libide-foundry.h'
