if get_option('with_ctags')

ctags_resources = gnome.compile_resources(
  'ctags-resources',
  'ctags.gresource.xml',
  c_name: 'ide_ctags',
)

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

gnome_builder_plugins_sources += files(ctags_sources)
gnome_builder_plugins_sources += ctags_resources[0]

endif
