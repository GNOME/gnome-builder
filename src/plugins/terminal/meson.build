terminal_resources = gnome.compile_resources(
  'terminal-resources',
  'terminal.gresource.xml',
  c_name: 'gb_terminal',
)

terminal_sources = [
  'gb-terminal-plugin.c',
  'gb-terminal-private.h',
  'gb-terminal-util.c',
  'gb-terminal-util.h',
  'gb-terminal-view.c',
  'gb-terminal-view.h',
  'gb-terminal-view-private.h',
  'gb-terminal-view-actions.c',
  'gb-terminal-view-actions.h',
  'gb-terminal-search.c',
  'gb-terminal-search.h',
  'gb-terminal-search-private.h',
  'gb-terminal-workbench-addin.c',
  'gb-terminal-workbench-addin.h',
]

gnome_builder_plugins_deps += [libvte_dep]

gnome_builder_plugins_sources += files(terminal_sources)
gnome_builder_plugins_sources += terminal_resources[0]
