# Needed for ggit-push-options
libgit2_glib_dep = dependency('libgit2-glib-1.0', version: '>= 1.1.0')

# Make sure libgit2/libgit2-glib were compiled with proper flags
libgit_thread_safe_check = '''
#include <libgit2-glib/ggit.h>
int main(int argc, const char *argv[])
{
ggit_init ();
return ((ggit_get_features() & GGIT_FEATURE_THREADS) != 0) ? 0 : 1;
}
'''
res = cc.run(libgit_thread_safe_check,
dependencies: libgit2_glib_dep,
)
if res.returncode() != 0
error('libgit2 was not compiled with -DUSE_THREADS:BOOL=ON')
endif

libgit_ssh_check = '''
#include <libgit2-glib/ggit.h>
int main(int argc, const char *argv[])
{
ggit_init ();
return ((ggit_get_features() & GGIT_FEATURE_SSH) != 0) ? 0 : 1;
}
'''
res = cc.run(libgit_ssh_check,
dependencies: libgit2_glib_dep,
)
if res.returncode() != 0
error('libgit2 was not compiled with SSH support')
endif

gnome_builder_git_deps = [
  libgiounix_dep,
  libgit2_glib_dep,
]

ipc_git_blame_src = gnome.gdbus_codegen('ipc-git-blame',
           sources: 'org.gnome.Builder.Git.Blame.xml',
  interface_prefix: 'org.gnome.Builder.',
         namespace: 'Ipc',
)

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
  'ipc-git-blame-impl.c',
  'ipc-git-change-monitor-impl.c',
  'ipc-git-index-monitor.c',
  'ipc-git-remote-callbacks.c',
  'ipc-git-repository-impl.c',
  'ipc-git-service-impl.c',
  'line-cache.c',
  ipc_git_blame_src,
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
  ipc_git_blame_src,
  ipc_git_change_monitor_src,
  ipc_git_config_src,
  ipc_git_progress_src,
  ipc_git_repository_src,
  ipc_git_service_src,
]

test_git = executable('test-git', 'test-git.c', test_git_sources,
  dependencies: [ libgiounix_dep ],
)

test_clone = executable('test-clone', 'test-clone.c', test_git_sources,
  dependencies: [ libgiounix_dep ],
)

# test('test-git', test_git)
