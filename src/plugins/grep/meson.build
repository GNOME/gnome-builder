if get_option('plugin_grep')

plugins_sources += files([
  'gbp-grep-model.c',
  'gbp-grep-panel.c',
  'gbp-grep-popover.c',
  'gbp-grep-tree-addin.c',
  'gbp-grep-workspace-addin.c',
  'grep-plugin.c',
])

plugin_grep_resources = gnome.compile_resources(
  'grep-resources',
  'grep.gresource.xml',
  c_name: 'gbp_grep',
)

plugins_sources += plugin_grep_resources

endif
