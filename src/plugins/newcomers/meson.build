if get_option('with_newcomers')

newcomers_resources = gnome.compile_resources(
  'newcomers-resources',
  'newcomers.gresource.xml',
  c_name: 'gbp_newcomers',
)

newcomers_sources = [
  'newcomers-plugin.c',
  'gbp-newcomers-project.c',
  'gbp-newcomers-project.h',
  'gbp-newcomers-section.c',
  'gbp-newcomers-section.h',
]

gnome_builder_plugins_sources += files(newcomers_sources)
gnome_builder_plugins_sources += newcomers_resources[0]

endif
