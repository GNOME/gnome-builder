if get_option('plugin_devhelp')

plugins_deps += [
  dependency('libdevhelp-3.0', version: '>=3.25.1'),
]

plugins_sources += files([
  'devhelp-plugin.c',
  'gbp-devhelp-editor-addin.c',
  'gbp-devhelp-frame-addin.c',
  'gbp-devhelp-hover-provider.c',
  'gbp-devhelp-menu-button.c',
  'gbp-devhelp-page.c',
  'gbp-devhelp-search.c',
])

plugin_devhelp_resources = gnome.compile_resources(
  'devhelp-resources',
  'devhelp.gresource.xml',
  c_name: 'gbp_devhelp',
)

plugins_sources += plugin_devhelp_resources

endif
