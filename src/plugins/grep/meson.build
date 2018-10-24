if get_option('with_grep')

grep_resources = gnome.compile_resources(
  'grep-resources',
  'grep.gresource.xml',
  c_name: 'gbp_grep',
)

grep_sources = [
  'gbp-grep-dialog.c',
  'gbp-grep-editor-addin.c',
  'gbp-grep-model.c',
  'gbp-grep-panel.c',
  'gbp-grep-plugin.c',
]

gnome_builder_plugins_sources += files(grep_sources)
gnome_builder_plugins_sources += grep_resources[0]

endif
