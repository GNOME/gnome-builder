if get_option('plugin_git')

plugins_sources += files([
  'git-plugin.c',
  'gbp-git-branch.c',
  'gbp-git-buffer-addin.c',
  'gbp-git-buffer-change-monitor.c',
  'gbp-git-client.c',
  'gbp-git-dependency-updater.c',
  'gbp-git-index-monitor.c',
  'gbp-git-pipeline-addin.c',
  'gbp-git-remote-callbacks.c',
  'gbp-git-submodule-stage.c',
  'gbp-git-tag.c',
  'gbp-git-vcs.c',
  'gbp-git-vcs-cloner.c',
  'gbp-git-vcs-config.c',
  'gbp-git-vcs-initializer.c',
  'gbp-git-workbench-addin.c',
  'line-cache.c',
])

gnome_builder_git_sources = [
  'gbp-git.c',
  'gbp-git-remote-callbacks.c',
  'gnome-builder-git.c',
  'line-cache.c',
]

plugin_git_resources = gnome.compile_resources(
  'git-resources',
  'git.gresource.xml',
  c_name: 'gbp_git',
)

plugins_sources += plugin_git_resources[0]

gnome_builder_git_deps = [
  libgit_dep,
  libjsonrpc_glib_dep,
  libgiounix_dep,
]

executable('gnome-builder-git', gnome_builder_git_sources,
      dependencies: gnome_builder_git_deps,
           gui_app: false,
           install: true,
       install_dir: get_option('libexecdir'),
            c_args: exe_c_args,
         link_args: exe_link_args,
     install_rpath: pkglibdir_abs,
)

test_line_cache = executable('test-line-cache', ['test-line-cache.c', 'line-cache.c'],
      dependencies: [libgio_dep],
           gui_app: false,
           install: false,
            c_args: exe_c_args,
         link_args: exe_link_args,
)
test('test-line-cache', test_line_cache)

endif
