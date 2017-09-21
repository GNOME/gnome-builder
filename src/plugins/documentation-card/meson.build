if get_option('with_documentation_card')

documentation_card_resources = gnome.compile_resources(
  'gbp-documentation-card-resources',
  'gbp-documentation-card-resources.gresource.xml',
  c_name: 'gbp_documentation_card',
)

documentation_card_sources = [
  'gbp-documentation-card.c',
  'gbp-documentation-card.h',
  'gbp-documentation-card-view-addin.c',
  'gbp-documentation-card-view-addin.h',
  'gbp-documentation-card-plugin.c',
  documentation_card_resources[0],
]

shared_module('documentation-card-plugin', documentation_card_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

install_data('documentation-card.plugin', install_dir: plugindir)

endif
