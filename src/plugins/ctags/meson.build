if get_option('with_ctags')

ctags_resources = gnome.compile_resources(
  'ctags-resources',
  'ctags.gresource.xml',
  c_name: 'ide_ctags',
)

ctags_sources = [
  'ide-ctags-builder.c',
  'ide-ctags-completion-item.c',
  'ide-ctags-completion-provider.c',
  'ide-ctags-highlighter.c',
  'ide-ctags-index.c',
  'ide-ctags-service.c',
  'ide-ctags-results.c',
  'ide-ctags-symbol-node.c',
  'ide-ctags-symbol-resolver.c',
  'ide-ctags-symbol-tree.c',
  'ide-ctags-util.c',
  'ctags-plugin.c',
]

gnome_builder_plugins_sources += files(ctags_sources)
gnome_builder_plugins_sources += ctags_resources[0]

endif
