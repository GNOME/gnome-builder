devices_headers = [
  'ide-deploy-strategy.h',
  'ide-device-info.h',
  'ide-device-manager.h',
  'ide-device-provider.h',
  'ide-device.h',
]

devices_sources = [
  'ide-deploy-strategy.c',
  'ide-device-info.c',
  'ide-device-manager.c',
  'ide-device-provider.c',
  'ide-device.c',
]

devices_enums = [
  'ide-device.h',
  'ide-device-info.h',
]

libide_enum_headers += files(devices_enums)
libide_public_headers += files(devices_headers)
libide_public_sources += files(devices_sources)

install_headers(devices_headers, subdir: join_paths(libide_header_subdir, 'devices'))
