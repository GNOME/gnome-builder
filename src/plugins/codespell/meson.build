if get_option('plugin_codespell')

plugins_sources += files([
  'codespell-plugin.c',
  'ide-codespell-diagnostic-provider.c',
])

plugin_codespell_resources = gnome.compile_resources(
  'codespell-resources',
  'codespell.gresource.xml',
  c_name: 'gbp_codespell',
)

plugins_sources += plugin_codespell_resources

endif
