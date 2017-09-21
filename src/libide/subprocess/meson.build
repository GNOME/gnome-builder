subprocess_headers = [
  'ide-subprocess.h',
  'ide-subprocess-launcher.h',
  'ide-subprocess-supervisor.h',
]

subprocess_sources = [
  'ide-subprocess.c',
  'ide-subprocess-launcher.c',
  'ide-subprocess-supervisor.c',
]

subprocess_private_sources = [
  'ide-breakout-subprocess.c',
  'ide-breakout-subprocess.h',
  'ide-breakout-subprocess-private.h',
  'ide-simple-subprocess.c',
  'ide-simple-subprocess.h',
]

libide_public_headers += files(subprocess_headers)
libide_public_sources += files(subprocess_sources)
libide_private_sources += files(subprocess_private_sources)

install_headers(subprocess_headers, subdir: join_paths(libide_header_subdir, 'subprocess'))
