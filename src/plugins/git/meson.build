if get_option('with_git')

git_resources = gnome.compile_resources(
  'git-resources',
  'git.gresource.xml',
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
]

gnome_builder_plugins_deps += [
  libgit_dep,
]

gnome_builder_plugins_sources += files(git_sources)
gnome_builder_plugins_sources += git_resources[0]

endif
