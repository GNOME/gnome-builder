if get_option('with_documentation_card')

documentation_card_resources = gnome.compile_resources(
  'documentation-card-resources',
  'documentation-card.gresource.xml',
  c_name: 'gbp_documentation_card',
)

documentation_card_sources = [
  'gbp-documentation-card.c',
  'gbp-documentation-card.h',
  'gbp-documentation-card-view-addin.c',
  'gbp-documentation-card-view-addin.h',
  'gbp-documentation-card-plugin.c',
]

gnome_builder_plugins_sources += files(documentation_card_sources)
gnome_builder_plugins_sources += documentation_card_resources[0]

endif
