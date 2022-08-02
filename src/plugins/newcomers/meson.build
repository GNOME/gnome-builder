if get_option('plugin_newcomers')

plugins_sources += files([
  'newcomers-plugin.c',
  'gbp-newcomers-project.c',
  'gbp-newcomers-section.c',
])

plugin_newcomers_resources = gnome.compile_resources(
  'newcomers-resources',
  'newcomers.gresource.xml',
  c_name: 'gbp_newcomers',
)

plugins_sources += plugin_newcomers_resources

subdir('icons')

endif
