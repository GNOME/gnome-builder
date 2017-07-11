if get_option('with_command_bar')

command_bar_resources = gnome.compile_resources(
  'gb-command-bar-resources',
  'gb-command-bar.gresource.xml',
  c_name: 'gb_command_bar',
)

command_bar_sources = [
  command_bar_resources[0],
  command_bar_resources[1],
  'gb-command-bar.c',
  'gb-command-bar.h',
  'gb-command-gaction-provider.c',
  'gb-command-gaction-provider.h',
  'gb-command-gaction.c',
  'gb-command-gaction.h',
  'gb-command-manager.c',
  'gb-command-manager.h',
  'gb-command-provider.c',
  'gb-command-provider.h',
  'gb-command-result.c',
  'gb-command-result.h',
  'gb-command-vim-provider.c',
  'gb-command-vim-provider.h',
  'gb-command-vim.c',
  'gb-command-vim.h',
  'gb-command.c',
  'gb-command.h',
  'gb-vim.c',
  'gb-vim.h',
]

command_bar_deps = plugin_deps + [
  libgd_dep,
]

shared_module('command-bar', command_bar_sources,
  dependencies: command_bar_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

configure_file(
          input: 'command-bar.plugin',
         output: 'command-bar.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
