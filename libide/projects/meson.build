projects_headers = [
  'ide-project-edit.h',
  'ide-project-file.h',
  'ide-project-files.h',
  'ide-project-info.h',
  'ide-project-item.h',
  'ide-project-miner.h',
  'ide-project.h',
  'ide-recent-projects.h',
]

projects_sources = [
  'ide-project-edit.c',
  'ide-project-file.c',
  'ide-project-files.c',
  'ide-project-info.c',
  'ide-project-item.c',
  'ide-project-miner.c',
  'ide-project.c',
  'ide-recent-projects.c',
]

projects_private_sources = [
  'ide-project-edit-private.h',
]

libide_public_headers += files(projects_headers)
libide_public_sources += files(projects_sources)
libide_private_sources += files(projects_private_sources)

install_headers(projects_headers, subdir: join_paths(libide_header_subdir, 'projects'))
