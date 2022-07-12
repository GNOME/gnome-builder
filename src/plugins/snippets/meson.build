plugins_sources += files([
  'snippets-plugin.c',
  'gbp-snippet-application-addin.c',
  'gbp-snippet-completion-provider.c',
])

snippets_resources = gnome.compile_resources(
  'snippets-resources',
  'snippets.gresource.xml',
  c_name: 'gbp_snippets',
)

plugins_sources += snippets_resources
