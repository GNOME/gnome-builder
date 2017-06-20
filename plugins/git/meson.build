if get_option('with_git')

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

configure_file(
          input: 'git.plugin',
         output: 'git.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
