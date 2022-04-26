plugins_sources += files([
  'snippets-plugin.c',
  'ide-snippet-application-addin.c',
  'ide-snippet-preferences-addin.c',
])

snippets_resources = gnome.compile_resources(
  'snippets-resources',
  'snippets.gresource.xml',
  c_name: 'gbp_snippets',
)

plugins_sources += snippets_resources
