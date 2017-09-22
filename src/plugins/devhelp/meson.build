if get_option('with_devhelp')

devhelp_resources = gnome.compile_resources(
  'devhelp-resources',
  'devhelp.gresource.xml',
  c_name: 'gbp_devhelp',
)

devhelp_sources = [
  'gbp-devhelp-documentation-provider.c',
  'gbp-devhelp-documentation-provider.h',
  'gbp-devhelp-menu-button.c',
  'gbp-devhelp-menu-button.h',
  'gbp-devhelp-layout-stack-addin.c',
  'gbp-devhelp-layout-stack-addin.h',
  'gbp-devhelp-editor-addin.c',
  'gbp-devhelp-editor-addin.h',
  'gbp-devhelp-editor-view-addin.c',
  'gbp-devhelp-editor-view-addin.h',
  'gbp-devhelp-plugin.c',
  'gbp-devhelp-search.c',
  'gbp-devhelp-search.h',
  'gbp-devhelp-search-private.h',
  'gbp-devhelp-view.c',
  'gbp-devhelp-view.h',
]

gnome_builder_plugins_deps += [
  dependency('libdevhelp-3.0', version: '>=3.25.1'),
]

gnome_builder_plugins_sources += files(devhelp_sources)
gnome_builder_plugins_sources += devhelp_resources[0]

endif
