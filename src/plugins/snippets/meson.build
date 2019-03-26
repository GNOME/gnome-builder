plugins_sources += files([
  'snippets-plugin.c',
  'ide-snippet-completion-provider.c',
  'ide-snippet-completion-item.c',
  'ide-snippet-model.c',
  'ide-snippet-preferences-addin.c',
])

snippets_resources = gnome.compile_resources(
  'snippets-resources',
  'snippets.gresource.xml',
  c_name: 'gbp_snippets',
)

plugins_sources += snippets_resources
