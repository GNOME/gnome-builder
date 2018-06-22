config_headers = [
  'ide-configuration-manager.h',
  'ide-configuration-provider.h',
  'ide-configuration.h',
  'ide-config-view.h',
  'ide-config-view-addin.h',
]

config_sources = [
  'ide-configuration-manager.c',
  'ide-configuration-provider.c',
  'ide-configuration.c',
  'ide-config-view.c',
  'ide-config-view-addin.c',
]

config_enums = [
  'ide-configuration.h',
]

libide_enum_headers += files(config_enums)
libide_public_headers += files(config_headers)
libide_public_sources += files(config_sources)

install_headers(config_headers, subdir: join_paths(libide_header_subdir, 'config'))
