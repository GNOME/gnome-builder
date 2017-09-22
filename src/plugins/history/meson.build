if get_option('with_history')

history_resources = gnome.compile_resources(
  'history-resources',
  'history.gresource.xml',
  c_name: 'gbp_history',
)

history_sources = [
  'gbp-history-layout-stack-addin.c',
  'gbp-history-layout-stack-addin.h',
  'gbp-history-editor-view-addin.c',
  'gbp-history-editor-view-addin.h',
  'gbp-history-item.c',
  'gbp-history-item.h',
  'gbp-history-plugin.c',
]

gnome_builder_plugins_sources += files(history_sources)
gnome_builder_plugins_sources += history_resources[0]

endif
