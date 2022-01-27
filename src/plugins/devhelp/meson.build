if get_option('plugin_devhelp')

if not get_option('webkit').enabled()
  error('WebKit must be enabled for devhelp support')
endif

if not libportal_dep.found()
  error('libportal is required for devhelp plugin support')
endif

plugins_deps += [
  dependency('libdevhelp-3.0', version: '>=3.25.1'),
  libportal_dep,
]

plugins_sources += files([
  'devhelp-plugin.c',
  'gbp-devhelp-editor-addin.c',
  'gbp-devhelp-frame-addin.c',
  'gbp-devhelp-hover-provider.c',
  'gbp-devhelp-menu-button.c',
  'gbp-devhelp-page.c',
  'gbp-devhelp-search.c',
  'gbp-devhelp-session-addin.c',
])

plugin_devhelp_resources = gnome.compile_resources(
  'devhelp-resources',
  'devhelp.gresource.xml',
  c_name: 'gbp_devhelp',
)

plugins_sources += plugin_devhelp_resources

endif
