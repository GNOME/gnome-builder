runner_headers = [
  'ide-run-button.h',
  'ide-run-manager.h',
  'ide-runner.h',
  'ide-runner-addin.h',
]

runner_sources = [
  'ide-run-button.c',
  'ide-run-manager.c',
  'ide-runner.c',
  'ide-runner-addin.c',
]

runner_private_sources = [
  'ide-run-manager-private.h',
]

libide_public_headers += files(runner_headers)
libide_public_sources += files(runner_sources)
libide_private_sources += files(runner_private_sources)

install_headers(runner_headers, subdir: join_paths(libide_header_subdir, 'runner'))
