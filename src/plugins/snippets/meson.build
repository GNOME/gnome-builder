if get_option('with_snippets')

snippets_resources = gnome.compile_resources(
  'snippets-resources',
  'snippets.gresource.xml',
  c_name: 'gbp_snippets',
)

snippets_sources = [
  'snippets-plugin.c',
  'ide-snippet-completion-provider.c',
  'ide-snippet-completion-item.c',
  'ide-snippet-model.c',
  'ide-snippet-preferences-addin.c',
]

gnome_builder_plugins_sources += files(snippets_sources)
gnome_builder_plugins_sources += snippets_resources[0]

endif
