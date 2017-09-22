if get_option('with_command_bar')

command_bar_resources = gnome.compile_resources(
  'gb-command-bar-resources',
  'gb-command-bar.gresource.xml',
  c_name: 'gb_command_bar',
)

command_bar_sources = [
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

gnome_builder_plugins_deps += [libgd_dep]
gnome_builder_plugins_sources += files(command_bar_sources)
gnome_builder_plugins_sources += command_bar_resources[0]

endif
