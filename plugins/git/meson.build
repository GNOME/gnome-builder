if get_option('with_git')

libgit_dep = dependency('libgit2-glib-1.0', version: '>=0.24.0')
cc = meson.get_compiler('c')

libgit_thread_safe_check = '
		#include <libgit2-glib/ggit.h>
		int
		main(int argc, const char *argv[])
		{
			ggit_init ();
			return ((ggit_get_features() & GGIT_FEATURE_THREADS) != 0) ? 0 : 1;
		}
'
res = cc.run(libgit_thread_safe_check,
  dependencies: libgit_dep,
)
if res.returncode() != 0
  error('libgit2 was not compiled with -DTHREADSAFE:BOOL=ON')
endif

libgit_ssh_check = '
		#include <libgit2-glib/ggit.h>
		int
		main(int argc, const char *argv[])
		{
			ggit_init ();
			return ((ggit_get_features() & GGIT_FEATURE_SSH) != 0) ? 0 : 1;
		}
'

res = cc.run(libgit_ssh_check,
  dependencies: libgit_dep,
)
if res.returncode() != 0
  error('libgit2 was not compiled with SSH support')
endif

git_resources = gnome.compile_resources(
  'ide-git-resources',
  'ide-git-resources.gresource.xml',
  c_name: 'ide_git',
)

git_sources = [
  'ide-git-buffer-change-monitor.c',
  'ide-git-buffer-change-monitor.h',
  'ide-git-clone-widget.c',
  'ide-git-clone-widget.h',
  'ide-git-genesis-addin.c',
  'ide-git-genesis-addin.h',
  'ide-git-plugin.c',
  'ide-git-remote-callbacks.c',
  'ide-git-remote-callbacks.h',
  'ide-git-vcs.c',
  'ide-git-vcs.h',
  'ide-git-vcs-config.c',
  'ide-git-vcs-config.h',
  'ide-git-vcs-initializer.c',
  'ide-git-vcs-initializer.h',
  git_resources[0],
]

git_deps = plugin_deps + [
  libgit_dep,
]

shared_module('git-plugin', git_sources,
  dependencies: git_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

install_data('git.plugin', install_dir: plugindir)

endif
