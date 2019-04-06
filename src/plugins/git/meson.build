if get_option('plugin_git')

subdir('daemon')

plugins_sources += files([
  'git-plugin.c',
  'gbp-git-branch.c',
  'gbp-git-buffer-addin.c',
  'gbp-git-buffer-change-monitor.c',
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

plugin_git_resources = gnome.compile_resources(
  'git-resources',
  'git.gresource.xml',
  c_name: 'gbp_git',
)

plugins_sources += plugin_git_resources

endif
