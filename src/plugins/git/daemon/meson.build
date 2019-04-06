gnome_builder_git_deps = [
  libgiounix_dep,
  libgit_dep,
]

ipc_git_change_monitor_src = gnome.gdbus_codegen('ipc-git-change-monitor',
           sources: 'org.gnome.Builder.Git.ChangeMonitor.xml',
  interface_prefix: 'org.gnome.Builder.',
         namespace: 'Ipc',
)

ipc_git_config_src = gnome.gdbus_codegen('ipc-git-config',
           sources: 'org.gnome.Builder.Git.Config.xml',
  interface_prefix: 'org.gnome.Builder.',
         namespace: 'Ipc',
)

ipc_git_progress_src = gnome.gdbus_codegen('ipc-git-progress',
           sources: 'org.gnome.Builder.Git.Progress.xml',
  interface_prefix: 'org.gnome.Builder.',
         namespace: 'Ipc',
)

ipc_git_repository_src = gnome.gdbus_codegen('ipc-git-repository',
           sources: 'org.gnome.Builder.Git.Repository.xml',
  interface_prefix: 'org.gnome.Builder.',
         namespace: 'Ipc',
)

ipc_git_service_src = gnome.gdbus_codegen('ipc-git-service',
           sources: 'org.gnome.Builder.Git.Service.xml',
  interface_prefix: 'org.gnome.Builder.',
         namespace: 'Ipc',
)

gnome_builder_git_sources = [
  'gnome-builder-git.c',
  'ipc-git-config-impl.c',
  'ipc-git-change-monitor-impl.c',
  'ipc-git-index-monitor.c',
  'ipc-git-remote-callbacks.c',
  'ipc-git-repository-impl.c',
  'ipc-git-service-impl.c',
  'line-cache.c',
  ipc_git_change_monitor_src,
  ipc_git_config_src,
  ipc_git_progress_src,
  ipc_git_repository_src,
  ipc_git_service_src,
]

gnome_builder_git = executable('gnome-builder-git', gnome_builder_git_sources,
           install: true,
       install_dir: get_option('libexecdir'),
      dependencies: gnome_builder_git_deps,
)

test_git_sources = [
  'test-git.c',
  ipc_git_change_monitor_src,
  ipc_git_config_src,
  ipc_git_progress_src,
  ipc_git_repository_src,
  ipc_git_service_src,
]

test_git = executable('test-git', 'test-git.c', test_git_sources,
  dependencies: [ libgiounix_dep ],
)

# test('test-git', test_git)
