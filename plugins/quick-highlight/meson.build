if get_option('with_quick_highlight')

quick_highlight_sources = [
  'gbp-quick-highlight-plugin.c',
  'gbp-quick-highlight-view-addin.c',
  'gbp-quick-highlight-view-addin.h',
  'gbp-quick-highlight-preferences.c',
  'gbp-quick-highlight-preferences.h',
]

shared_module('quick-highlight-plugin', quick_highlight_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

configure_file(
          input: 'quick-highlight.plugin',
         output: 'quick-highlight.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
