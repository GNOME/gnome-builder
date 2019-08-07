plugins_sources += files([
  'command-bar-plugin.c',
  'gbp-command-bar.c',
  'gbp-command-bar-command-provider.c',
  'gbp-command-bar-shortcuts.c',
  'gbp-command-bar-suggestion.c',
  'gbp-command-bar-workspace-addin.c',
  'gbp-gaction-command.c',
])

plugin_command_bar_resources = gnome.compile_resources(
  'gbp-command-bar-resources',
  'command-bar.gresource.xml',
  c_name: 'gbp_command_bar',
)

plugins_sources += plugin_command_bar_resources
