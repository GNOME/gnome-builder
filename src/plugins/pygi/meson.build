if get_option('plugin_pygi')

plugins_sources += files([
  'pygi-plugin.c',
  'gbp-pygi-completion-provider.c',
  'gbp-pygi-proposals.c',
  'gbp-pygi-proposal.c',
])

plugin_pygi_resources = gnome.compile_resources(
  'pygi-resources',
  'pygi.gresource.xml',
  c_name: 'gbp_pygi',
)

plugins_sources += plugin_pygi_resources

endif
