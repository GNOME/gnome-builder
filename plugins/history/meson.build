if get_option('with_history')

history_sources = [
  'gbp-history-layout-stack-addin.c',
  'gbp-history-layout-stack-addin.h',
  'gbp-history-editor-view-addin.c',
  'gbp-history-editor-view-addin.h',
  'gbp-history-item.c',
  'gbp-history-item.h',
  'gbp-history-plugin.c',
]

shared_module('history-plugin', history_sources,
  dependencies: plugin_deps,
     link_args: plugin_link_args,
  link_depends: plugin_link_deps,
       install: true,
   install_dir: plugindir,
 install_rpath: pkglibdir_abs,
)

configure_file(
          input: 'history.plugin',
         output: 'history.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
