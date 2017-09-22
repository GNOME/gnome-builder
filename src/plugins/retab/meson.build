if get_option('with_retab')

retab_resources = gnome.compile_resources(
  'gbp-retab-resources',
  'retab.gresource.xml',
  c_name: 'gbp_retab',
)

retab_sources = [
  'gbp-retab-plugin.c',
  'gbp-retab-view-addin.c',
  'gbp-retab-view-addin.h',
]

gnome_builder_plugins_sources += files(retab_sources)
gnome_builder_plugins_sources += retab_resources[0]

endif
