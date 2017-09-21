if get_option('with_terminal')

terminal_resources = gnome.compile_resources(
  'gb-terminal-resources',
  'gb-terminal.gresource.xml',
  c_name: 'gb_terminal',
)

terminal_sources = [
  terminal_resources,
  'gb-terminal.c',
  'gb-terminal.h',
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

terminal_deps = plugin_deps + [
  dependency('vte-2.91', version: '>=0.40.2'),
  libgd_dep,
]

shared_module('terminal', terminal_sources,
  dependencies: terminal_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
  install_rpath: pkglibdir_abs,
)

configure_file(
          input: 'terminal.plugin',
         output: 'terminal.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
