buildconfig_headers = [
  'ide-buildconfig-configuration.h',
]

buildconfig_sources = [
  'ide-buildconfig-configuration.c',
]

buildconfig_private_sources = [
  'ide-buildconfig-configuration-provider.c',
  'ide-buildconfig-plugin.c',
  'ide-buildconfig-pipeline-addin.c',
  'ide-buildconfig-pipeline-addin.h',
]

libide_public_headers += files(buildconfig_headers)
libide_public_sources += files(buildconfig_sources)
libide_private_sources += files(buildconfig_private_sources)

install_headers(buildconfig_headers, subdir: join_paths(libide_header_subdir, 'buildconfig'))
