if get_option('with_glade')

glade_resources = gnome.compile_resources(
  'glade-resources',
  'glade.gresource.xml',
  c_name: 'gbp_glade',
)

glade_sources = [
  'gbp-glade-editor-addin.c',
  'gbp-glade-layout-stack-addin.c',
  'gbp-glade-plugin.c',
  'gbp-glade-properties.c',
  'gbp-glade-view.c',
  'gbp-glade-view-actions.c',
]

gnome_builder_plugins_deps += [
  dependency('gladeui-2.0', version: '>=3.22.0'),
]

gnome_builder_plugins_sources += files(glade_sources)
gnome_builder_plugins_sources += glade_resources[0]

endif
