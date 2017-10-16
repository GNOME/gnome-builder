buildsystem_headers = [
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
  'ide-build-target.h',
  'ide-build-utils.h',
  'ide-compile-commands.h',
  'ide-configuration-manager.h',
  'ide-configuration-provider.h',
  'ide-configuration.h',
  'ide-environment-variable.h',
  'ide-environment.h',
]

buildsystem_sources = [
  'ide-build-manager.c',
  'ide-build-pipeline-addin.c',
  'ide-build-pipeline.c',
  'ide-build-stage-launcher.c',
  'ide-build-stage-mkdirs.c',
  'ide-build-stage-transfer.c',
  'ide-build-stage.c',
  'ide-build-system-discovery.c',
  'ide-build-system.c',
  'ide-build-target.c',
  'ide-build-utils.c',
  'ide-compile-commands.c',
  'ide-configuration-manager.c',
  'ide-configuration-provider.c',
  'ide-configuration.c',
  'ide-environment-variable.c',
  'ide-environment.c',
]

buildsystem_private_sources = [
  'ide-build-log.c',
  'ide-build-log-private.h',
  'ide-build-stage-private.h',
]

buildsystem_enums = [
  'ide-build-log.h',
  'ide-build-pipeline.h',
]

libide_enum_headers += files(buildsystem_enums)
libide_public_headers += files(buildsystem_headers)
libide_public_sources += files(buildsystem_sources)
libide_private_sources += files(buildsystem_private_sources)

install_headers(buildsystem_headers, subdir: join_paths(libide_header_subdir, 'buildsystem'))
