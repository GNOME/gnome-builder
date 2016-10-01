if get_option('with_clang')

clang_sources = [
  'ide-clang-completion-item.c',
  'ide-clang-completion-item.h',
  'ide-clang-completion-item-private.h',
  'ide-clang-completion-provider.c',
  'ide-clang-completion-provider.h',
  'ide-clang-diagnostic-provider.c',
  'ide-clang-diagnostic-provider.h',
  'ide-clang-highlighter.c',
  'ide-clang-highlighter.h',
  'ide-clang-preferences-addin.c',
  'ide-clang-preferences-addin.h',
  'ide-clang-private.h',
  'ide-clang-service.c',
  'ide-clang-service.h',
  'ide-clang-symbol-node.c',
  'ide-clang-symbol-node.h',
  'ide-clang-symbol-resolver.c',
  'ide-clang-symbol-resolver.h',
  'ide-clang-symbol-tree.c',
  'ide-clang-symbol-tree.h',
  'ide-clang-translation-unit.c',
  'ide-clang-translation-unit.h',
  'clang-plugin.c',
]

cc = meson.get_compiler('c')
libclang = cc.find_library('libclang')

clang_deps = plugin_deps + [
  libclang,
]

shared_module('clang-plugin', clang_sources,
  dependencies: clang_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

install_data('clang.plugin', install_dir: plugindir)

endif
