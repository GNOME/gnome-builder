if get_option('with_ls')

grep_resources = gnome.compile_resources(
  'ls-resources',
  'ls.gresource.xml',
  c_name: 'gbp_ls',
)

grep_sources = [
  'gbp-ls-model.c',
  'gbp-ls-plugin.c',
  'gbp-ls-view.c',
  'gbp-ls-workbench-addin.c',
]

gnome_builder_plugins_sources += files(grep_sources)
gnome_builder_plugins_sources += grep_resources[0]

endif
