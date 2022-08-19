if get_option('plugin_ctags')

plugins_sources += files([
  'ctags-plugin.c',
  'ide-ctags-builder.c',
  'ide-ctags-completion-item.c',
  'ide-ctags-completion-provider.c',
  'ide-ctags-highlighter.c',
  'ide-ctags-index.c',
  'ide-ctags-results.c',
  'ide-ctags-service.c',
  'ide-ctags-symbol-node.c',
  'ide-ctags-symbol-resolver.c',
  'ide-ctags-symbol-tree.c',
  'ide-ctags-util.c',
  'ide-tags-builder.c',
  'gbp-ctags-tweaks-addin.c',
  'gbp-ctags-workbench-addin.c',
])

plugin_ctags_resources = gnome.compile_resources(
  'gbp-ctags-resources',
  'ctags.gresource.xml',
  c_name: 'gbp_ctags',
)

plugins_sources += plugin_ctags_resources

test_ctags = executable('test-ctags',
  'test-ctags.c', 'ide-ctags-index.c',
        c_args: test_cflags,
  dependencies: [ libide_projects_dep ],
)
test('test-ctags', test_ctags, env: test_env)

endif
