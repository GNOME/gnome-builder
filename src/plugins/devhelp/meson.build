if get_option('with_devhelp')

devhelp_resources = gnome.compile_resources(
  'devhelp-resources',
  'devhelp.gresource.xml',
  c_name: 'gbp_devhelp',
)

devhelp_sources = [
  'gbp-devhelp-menu-button.c',
  'gbp-devhelp-hover-provider.c',
  'gbp-devhelp-layout-stack-addin.c',
  'gbp-devhelp-editor-addin.c',
  'gbp-devhelp-plugin.c',
  'gbp-devhelp-search.c',
  'gbp-devhelp-view.c',
]

gnome_builder_plugins_deps += [
  dependency('libdevhelp-3.0', version: '>=3.25.1'),
]

gnome_builder_plugins_sources += files(devhelp_sources)
gnome_builder_plugins_sources += devhelp_resources[0]

endif
