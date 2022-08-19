plugins_sources += files([
  'gbp-terminal-tweaks-addin.c',
  'gbp-terminal-workspace-addin.c',
  'terminal-plugin.c',
])

plugin_terminal_resources = gnome.compile_resources(
  'gbp-terminal-resources',
  'terminal.gresource.xml',
  c_name: 'gbp_terminal',
)

plugins_sources += plugin_terminal_resources
