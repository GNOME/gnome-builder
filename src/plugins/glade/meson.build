if get_option('plugin_glade')

plugins_sources += files([
  'gbp-glade-editor-addin.c',
  'gbp-glade-frame-addin.c',
  'gbp-glade-properties.c',
  'gbp-glade-page.c',
  'gbp-glade-page-actions.c',
  'gbp-glade-page-shortcuts.c',
  'glade-plugin.c',
  'gbp-glade-workbench-addin.c',
])

plugin_glade_resources = gnome.compile_resources(
  'glade-resources',
  'glade.gresource.xml',
  c_name: 'gbp_glade',
)

plugins_deps += [
  dependency('gladeui-2.0', version: '>=3.22.0'),
]

plugins_sources += plugin_glade_resources

endif
