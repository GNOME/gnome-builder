testing_headers = [
  'ide-test.h',
  'ide-test-manager.h',
  'ide-test-provider.h',
]

testing_sources = [
  'ide-test.c',
  'ide-test-manager.c',
  'ide-test-provider.c',
]

testing_private_sources = [
  'ide-test-panel.c',
  'ide-test-editor-addin.c',
  'testing-plugin.c',
]

testing_enum_headers = [
  'ide-test.h',
]

libide_public_headers += files(testing_headers)
libide_public_sources += files(testing_sources)
libide_private_sources += files(testing_private_sources)
libide_enum_headers += files(testing_enum_headers)

install_headers(testing_headers, subdir: join_paths(libide_header_subdir, 'testing'))
