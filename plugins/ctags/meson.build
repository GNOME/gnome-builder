if get_option('with_ctags')

ctags_sources = [
  'ide-ctags-builder.c',
  'ide-ctags-builder.h',
  'ide-ctags-completion-item.c',
  'ide-ctags-completion-item.h',
  'ide-ctags-completion-provider.c',
  'ide-ctags-completion-provider.h',
  'ide-ctags-completion-provider-private.h',
  'ide-ctags-highlighter.c',
  'ide-ctags-highlighter.h',
  'ide-ctags-index.c',
  'ide-ctags-index.h',
  'ide-ctags-service.c',
  'ide-ctags-service.h',
  'ide-ctags-symbol-node.c',
  'ide-ctags-symbol-resolver.c',
  'ide-ctags-symbol-resolver.h',
  'ide-ctags-symbol-tree.c',
  'ide-ctags-util.c',
  'ide-ctags-util.h',
  'ctags-plugin.c',
]

shared_module('ctags-plugin', ctags_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

configure_file(
          input: 'ctags.plugin',
         output: 'ctags.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif
